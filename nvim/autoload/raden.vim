" ============================================================================
" raden.vim  —  core library for the Raden plugin
"
" Author: rayden
"
" Pure VimL functions used by the front-end commands in ftplugin/raden.vim,
" by the folding module and by the indentation module.  Everything here can
" be called any time (autoloaded on demand), works in both Vim and Neovim.
"
" Public functions (invoked with the raden# prefix):
"   raden#exec()            interpreter path ('' if nothing usable found)
"   raden#run(extra)        run the current buffer through the interpreter
"   raden#eval(l1,l2)       run lines l1..l2 of the buffer as a script
"   raden#tags()            formatted tag list across open *.rdn buffers
"   raden#tagfunc(...)      'tagfunc' backend for :tag / <C-]>
"   raden#complete(...)     'completefunc' backend (omnifunc)
"   raden#striplines()      buffer lines with comments/strings blanked
"   raden#strip(line)       single-line stripper for comments/strings
"   raden#is_block_open()   line ends in a block opener (if/loop/defun/...)
"   raden#is_block_close()  line starts with end/else
"
" Options honoured:
"   g:raden_exec            explicit interpreter path (overrides detection)
" ============================================================================
"
" The one global guard; autoload files are loaded explicitly by function
" calls so a guard is mostly belt-and-braces, but it keeps `script_load`
" semantics tidy when the file is :source'd directly.
if exists('g:raden_loaded_autoload')
  finish
endif
let g:raden_loaded_autoload = 1

" ---------------------------------------------------------------------------
" Executable detection
" ---------------------------------------------------------------------------
"
" Priority, matching the documented behaviour:
"   1. g:raden_exec            - explicitly chosen by the user
"   2. 'rdn'                   - installed interpreter found via $PATH
"   3. <buffer dir>.../main    - in-repo development build (walked upward)
" Returns '' when nothing usable exists; callers decide how to report it.

function! raden#exec() abort
  if exists('g:raden_exec') && !empty(g:raden_exec)
    return g:raden_exec
  endif

  if executable('rdn')
    return 'rdn'
  endif

  let l:dir = getcwd()
  if bufexists('%')
    let l:dir = expand('%:p:h')
  endif
  if empty(l:dir)
    let l:dir = getcwd()
  endif
  while 1
    let l:cand = l:dir . '/main'
    if executable(l:cand)
      return l:cand
    endif
    let l:next = fnamemodify(l:dir, ':h')
    if l:next ==# l:dir
      break
    endif
    let l:dir = l:next
  endwhile

  return ''
endfunction

" ---------------------------------------------------------------------------
" Comment / string stripping
" ---------------------------------------------------------------------------
"
" Raden block comments are [* ... *] (they may span lines and nest) and
" strings are " ... " with backslash escapes.  The strippers replace the
" *content* of such regions with spaces while keeping their width, so later
" keyword scanning only ever sees real program text and column alignment for
" the indent module is preserved.

" Whole-buffer stripper (span-aware).  Keeps count of the open [* state and
" the in-string state across lines.
function! s:strip_buffer(lines) abort
  let l:out = []
  let l:intag = 0
  let l:instr = 0

  for l:raw in a:lines
    let l:n = len(l:raw)
    let l:res = ''
    let l:i = 0
    while l:i < l:n
      let l:c = l:raw[l:i]
      if l:instr
        " \ escape swallows the next char (so \" does not close the string)
        if l:c ==# '\' && l:i + 1 < l:n
          let l:res .= '  '
          let l:i += 2
          continue
        elseif l:c ==# '"'
          let l:res .= ' '
          let l:instr = 0
        else
          let l:res .= ' '
        endif
      elseif l:intag > 0
        " comment content; only *] reduces the nesting depth
        if l:c ==# '*' && l:i + 1 < l:n && l:raw[l:i + 1] ==# ']'
          let l:res .= '  '
          let l:intag -= 1
          let l:i += 2
          continue
        else
          let l:res .= ' '
        endif
      else
        if l:c ==# '[' && l:i + 1 < l:n && l:raw[l:i + 1] ==# '*'
          let l:res .= '  '
          let l:intag += 1
          let l:i += 2
          continue
        elseif l:c ==# '"'
          let l:res .= ' '
          let l:instr = 1
        else
          let l:res .= l:c
        endif
      endif
      let l:i += 1
    endwhile
    call add(l:out, l:res)
  endfor

  return l:out
endfunction

" Cached per-buffer stripped lines; invalidated via b:changedtick.
let s:strip_cache = {}
function! raden#striplines() abort
  return s:striplines_for(bufnr('%'))
endfunction

function! s:striplines_for(buf) abort
  let l:tick = getbufvar(a:buf, 'changedtick')
  if has_key(s:strip_cache, a:buf) && s:strip_cache[a:buf].tick == l:tick
    return s:strip_cache[a:buf].lines
  endif
  let l:lines = s:strip_buffer(getbufline(a:buf, 1, '$'))
  let s:strip_cache[a:buf] = { 'tick': l:tick, 'lines': l:lines }
  return l:lines
endfunction

" Stateless single-line variant (multi-line comments are handled by the
" buffer-level version above).
function! raden#strip(line) abort
  let l:l = substitute(a:line, '"[^"]*"', '""', 'g')
  let l:l = substitute(l:l, '\[\*[^*]*\*\]', '', 'g')
  let l:l = substitute(l:l, '\[\*.*$', '', '')
  return l:l
endfunction

" ---------------------------------------------------------------------------
" Block vocabulary
" ---------------------------------------------------------------------------
"
" Raden is postfix: a block *begins* with the keyword that ends its line
" (if / loop / defun / module / apply) and is *closed* by a later `end`
" token.  `else` re-arms an if-body without changing the nesting.  These word
" lists drive folding, indentation and the module-aware completion.

let s:open_words = ['if', 'loop', 'defun', 'module', 'apply', 'demac']

function! raden#is_block_open(line) abort
  return a:line =~# '\<\%(if\|loop\|defun\|module\|apply\|demac\)\>\s*$'
endfunction

function! raden#is_block_close(line) abort
  return a:line =~# '^\s*\<\%(end\|else\)\>'
endfunction

" Net depth delta contributed by one stripped line: openers minus `end`s.
" `else` is deliberately neutral here (it forks, it does not close).
function! s:line_delta(line) abort
  let l:o = 0
  let l:c = 0
  for l:w in split(a:line)
    if index(s:open_words, l:w) >= 0
      let l:o += 1
    elseif l:w ==# 'end'
      let l:c += 1
    endif
  endfor
  return l:o - l:c
endfunction

" Public wrapper over the same counter (handy for custom folds/indents).
function! raden#block_delta(line) abort
  return s:line_delta(a:line)
endfunction

" Count of end/else tokens in a stripped line (used to lower fold/indent).
function! s:closers_count(line) abort
  let l:n = 0
  for l:w in split(a:line)
    if l:w ==# 'end' || l:w ==# 'else'
      let l:n += 1
    endif
  endfor
  return l:n
endfunction

" Public wrapper: number of closing words on the line.
function! raden#closers_count(line) abort
  return s:closers_count(a:line)
endfunction

" ---------------------------------------------------------------------------
" Running
" ---------------------------------------------------------------------------

" Run the current file through rdn.
"   a:extra  extra command-line arguments (the :RadenRun [args] tail)
" Returns the shell exit code (or 1 when nothing could run and echoes why).
" Side effects:
"   - program stdout is shown in a "[Raden Output]" scratch buffer
"   - rdn stderr is run through the active errorformat into the quickfix
"     list; when the run produced diagnostics those are opened (and jumped
"     to, when the exit code indicates failure).
function! raden#run(extra) abort
  let l:file = expand('%:p')
  if empty(l:file)
    call s:error('needs a buffer backed by a file (use %:%RadenRun)')
    return 1
  endif
  return s:run_file(l:file, a:extra)
endfunction

" Shared driver: run `file` (plus `extra` args), then route stdout/stderr.
function! s:run_file(file, extra) abort
  let l:exec = raden#exec()
  if empty(l:exec)
    call s:error('no Raden interpreter found (set g:raden_exec or install rdn)')
    return 1
  endif

  let l:outfile = tempname()
  let l:errfile = tempname()

  let l:cmd = shellescape(l:exec) . ' ' . shellescape(a:file)
  if !empty(a:extra)
    let l:cmd .= ' ' . a:extra
  endif
  let l:cmd .= ' >' . shellescape(l:outfile) . ' 2>' . shellescape(l:errfile)

  silent! call system(l:cmd)
  let l:code = v:shell_error

  let l:errors = []
  try
    let l:errors = readfile(l:errfile)
  catch
  endtry

  " stdout -> scratch buffer
  let l:out = []
  try
    let l:out = readfile(l:outfile)
  catch
  endtry
  call delete(l:outfile)
  if !empty(l:out)
    call s:show_out(l:out)
  endif

  " stderr -> quickfix list (or reset the list when the run was clean)
  if empty(l:errors)
    silent! call setqflist([], 'r')
    silent! cclose
  else
    " Parse rdn's diagnostic lines directly instead of depending on the
    " active errorformat / :cfile buffer interplay.  Format:
    "   /path/file.rdn:LINE:COL: error: message
    "   /path/file.rdn:LINE:COL: note: message
    " (followed by the echoed source line and a ^^^ caret line, skipped).
    let l:items = []
    for l:line in l:errors
      let l:m = matchlist(l:line,
            \ '\v^(.{-}):(\d+):(\d+): (error|note): (.*)$')
      if empty(l:m)
        continue
      endif
      call add(l:items, {
            \ 'filename': l:m[1],
            \ 'lnum': str2nr(l:m[2]),
            \ 'col': str2nr(l:m[3]),
            \ 'type': l:m[4] ==# 'error' ? 'E' : 'I',
            \ 'text': l:m[5],
            \ 'valid': 1,
            \ })
    endfor
    if empty(l:items)
      silent! call setqflist([], 'r')
      silent! cclose
    else
      call setqflist([], 'r', { 'items': l:items })
      copen
      if l:code != 0
        silent! cc 1
      endif
    endif
  endif

  call delete(l:errfile)
  return l:code
endfunction

" Pretty error message (keeps the plugin friendly to newcomers).
function! s:error(msg) abort
  echohl ErrorMsg
  echomsg 'raden: ' . a:msg
  echohl None
endfunction

" Public plain-message helper (used by the front-end commands).
function! raden#notify(msg) abort
  echohl Comment
  echomsg 'raden: ' . a:msg
  echohl None
endfunction

" Show stdout in a reused scratch buffer named "[Raden Output]".
function! s:show_out(lines) abort
  let l:name = '[Raden Output]'
  let l:buf = bufnr('^' . l:name . '$')
  if l:buf > 0 && bufexists(l:buf)
    execute 'silent buffer ' . l:buf
  else
    keepalt new
    execute 'silent file ' . fnameescape(l:name)
    setlocal buftype=nofile
    setlocal bufhidden=wipe
    setlocal noswapfile
    setlocal nobuflisted
  endif

  setlocal modifiable
  silent 1,$delete_
  call setline(1, a:lines)
  setlocal nomodifiable
  setlocal noreadonly
  normal! gg
endfunction

" ---------------------------------------------------------------------------
" Selection evaluation
" ---------------------------------------------------------------------------

" Run lines l1..l2 of the current buffer as a script.  The slice is written
" to a .rdn tempfile and executed; the tempfile is removed afterwards.
" Quickfix entries that referenced the temp path are renumbered back onto the
" source selection so `cc` / <CR> still lands inside the original buffer.
function! raden#eval(l1, l2) abort
  if a:l1 > a:l2
    return 1
  endif

  let l:exec = raden#exec()
  if empty(l:exec)
    call s:error('no Raden interpreter found (set g:raden_exec or install rdn)')
    return 1
  endif

  let l:srcbuf = bufnr('%')
  let l:srcfile = expand('%:p')
  let l:tmp = tempname() . '.rdn'
  call writefile(getline(a:l1, a:l2), l:tmp, 'b')
  let l:code = s:run_file(l:tmp, '')

  " The quickfix items we built reference the temp slice; renumber them onto
  " the original buffer+lines so `cc` / <CR> land in the source file.
  let l:tmpbuf = bufnr(l:tmp)
  let l:items = getqflist()
  let l:changed = 0
  for l:item in l:items
    if l:item.bufnr == l:tmpbuf
      let l:item.bufnr = l:srcbuf
      let l:item.lnum = a:l1 + l:item.lnum - 1
      let l:item.lnum = min([l:item.lnum, a:l2])
      let l:changed = 1
    endif
  endfor
  if l:changed
    call setqflist([], 'r', { 'items': l:items })
  endif

  " Drop the transient temp buffer (copen may have switched to it) and return
  " to the source file.
  if l:tmpbuf > 0 && bufexists(l:tmpbuf) && l:tmpbuf != bufnr('%')
    execute 'silent! bwipeout ' . l:tmpbuf
  endif
  silent execute 'buffer ' . l:srcbuf

  call delete(l:tmp)
  return l:code
endfunction

" ---------------------------------------------------------------------------
" Tags
" ---------------------------------------------------------------------------

" Scan every listed *.rdn buffer for tag-shaped lines:
"   name defun     -> tag `name`
"   Name module    -> tag `Name`
"   @name apply    -> tag `@name`
" Returns a list suitable for :tselect via 'tagfunc'.
function! raden#tagfunc(pattern, flags, info) abort
  if a:flags =~# 'i'
    return []
  endif

  let l:tags = []
  for l:buf in getbufinfo({ 'buflisted': v:true })
    if l:buf.name !~# '\.rdn$'
      continue
    endif
    for l:line in getbufline(l:buf.bufnr, 1, '$')
      " Postfix declarations: the tag name is the *leading* token of the
      " line (`format defun`, `@not apply`, `drop demac`).  Signed defuns
      " put their sig between name and keyword:
      "   sqrt (Real) (Real) defun sqrtOf call end  ->  tag `sqrt`
      " so "token before keyword" alone would be wrong, hence leading-token.
      if l:line =~# '\<\%(defun\|module\|apply\|demac\)\>'
        let l:name = matchstr(l:line, '^\s*\zs\S\+')
        if l:name !=# '' && l:name !~# '^[0-9]'
          call add(l:tags, s:mk_tag(l:name, l:buf, l:line))
        endif
      endif
    endfor
  endfor

  if a:pattern !=# '' && a:flags =~# 'c'
    let l:pat = '^' . escape(a:pattern, '\\^$.*[]{}') . '$'
    call filter(l:tags, 'v:val.name =~# l:pat')
  endif

  return l:tags
endfunction

" One tag dict.  The `cmd` is a /pattern/ search Vim uses to jump; storing the
" byte-exact raw line keeps :tag accurate even for freshly modified buffers.
function! s:mk_tag(name, bufinfo, line) abort
  let l:pat = escape(a:line, '\|/')
  return {
        \ 'name': a:name,
        \ 'filename': fnamemodify(a:bufinfo.name, ':p'),
        \ 'cmd': '/\V' . l:pat . '/',
        \ }
endfunction

" Formatted listing for :RadenTags (one tag per line).
function! raden#tags() abort
  let l:res = []
  for l:tag in raden#tagfunc('', 'c', {})
    call add(l:res, printf('%-24s %s', l:tag.name, fnamemodify(l:tag.filename, ':~:.')))
  endfor
  return l:res
endfunction

" ---------------------------------------------------------------------------
" Completion
" ---------------------------------------------------------------------------

" Static vocabulary; deliberately mirrors syntax/raden.vim (kept in lockstep).
let s:keywords = [ 'let', 'const', 'enum', 'reset', 'demac', 'defun', 'apply',
      \ 'call', 'set', 'pcall', 'unlet', 'ret', 'module', 'open',
      \ 'if', 'else', 'end', 'loop', 'break', 'continue' ]

let s:builtins = [ 'print', 'type', 'exit', 'pop', 'swap', 'dup', 'to_string',
      \ 'load', 'loadnative', 'add_load_path', 'add_native_path', 'error',
      \ 'do_string', 'do_file', 'match', 'assert', 'append', 'remove',
      \ 'index', 'len', '__argv', '__host_os', '__sharedlib_ext', '__line_col',
      \ '__file', '__func_name', '__stack_size' ]

let s:types = [ 'Int', 'Str', 'Bool', 'Real', 'List', 'Null' ]

" Build one completion record.
function! s:word(text) abort
  return { 'word': a:text, 'icase': 1, 'dup': 0 }
endfunction

" Shape of the per-buffer symbol cache.
let s:sym_cache = {}

" Harvest identifiers from one buffer.  Returns:
"   names    - tokens preceding defun / let / const / module
"   tags     - every @name token
"   members  - { Module: [member, ...] } for defuns lexically inside a module
" Module attribution walks the nesting: any `end` pops back to the previous
" depth, which matches how rdn lays its modules out in practice.
function! s:symbols(bufnr) abort
  let l:buffick = getbufvar(a:bufnr, 'changedtick')
  if has_key(s:sym_cache, a:bufnr)
        \ && s:sym_cache[a:bufnr].tick == l:buffick
    return s:sym_cache[a:bufnr].data
  endif

  let l:data = { 'names': [], 'tags': [], 'members': {} }
  let l:slines = s:striplines_for(a:bufnr)
  let l:modstack = []          " [ {'name','path','depth'}, ... ]
  let l:depth = 0              " net block depth across ALL openers

  for l:idx in range(1, len(l:slines))
    let l:line = l:slines[l:idx - 1]

    " counter pass over the stripped words
    let l:nmod = 0
    let l:modules = 0
    let l:ends = 0
    for l:w in split(l:line)
      if l:w ==# 'module'
        let l:modules += 1
      elseif index(s:open_words, l:w) >= 0
        let l:nmod += 1
      elseif l:w ==# 'end'
        let l:ends += 1
      endif
    endfor
    let l:delta = l:modules + l:nmod - l:ends

    " named definitions - the leading token of declaration lines.  A name is a
    " module *member* only when the line sits exactly at the module's own body
    " depth (not inside a nested defun/apply/... body).
    if l:line =~# '\<\%(defun\|apply\|module\|demac\|let\|const\)\>'
      let l:name = matchstr(l:line, '^\s*\zs\S\+')
      if l:name !=# '' && l:name !~# '^[0-9(]'
        call add(l:data.names, l:name)
        if !empty(l:modstack) && l:depth == l:modstack[-1].depth
          let l:mod = l:modstack[-1].path
          if !has_key(l:data.members, l:mod)
            let l:data.members[l:mod] = []
          endif
          call add(l:data.members[l:mod], l:name)
        endif
      endif
    endif

    " @tags (names may contain '-', matching identifier rules)
    let l:off = 0
    while 1
      let l:off = match(l:line, '@[-\w]\+', l:off)
      if l:off < 0
        break
      endif
      let l:full = matchstr(l:line, '@[-\w]\+', l:off)
      if index(l:data.tags, l:full) < 0
        call add(l:data.tags, l:full)
      endif
      let l:off += len(l:full)
    endwhile

    " update depth AFTER this line's definitions
    let l:depth += l:delta

    " modules whose block depth fell back below their opening depth are done
    while !empty(l:modstack) && l:modstack[-1].depth > l:depth
      call remove(l:modstack, -1)
    endwhile

    " a module opener line starts a new module scope at the (new) depth
    if l:modules > 0
      let l:modname = matchstr(l:line, '^\s*\zs\S\+')
      if l:modname !=# '' && l:modname !~# '^[0-9(]'
        let l:path = empty(l:modstack) ? l:modname : l:modstack[-1].path . '::' . l:modname
        call add(l:modstack, { 'name': l:modname, 'path': l:path, 'depth': l:depth })
      endif
    endif
  endfor

  let s:sym_cache[a:bufnr] = { 'tick': l:buffick, 'data': l:data }
  return l:data
endfunction

" Public wrapper: symbol harvest for one buffer (see s:symbols).
function! raden#symbols(bufnr) abort
  return s:symbols(a:bufnr)
endfunction

" Merge the symbol harvest across all listed *.rdn buffers.
function! s:all_symbols() abort
  let l:merged = { 'names': [], 'tags': [], 'members': {} }
  for l:info in getbufinfo({ 'buflisted': v:true })
    if l:info.name !~# '\.rdn$'
      continue
    endif
    let l:s = s:symbols(l:info.bufnr)
    call extend(l:merged.names, l:s.names)
    call extend(l:merged.tags, l:s.tags)
    call extend(l:merged.members, l:s.members)
  endfor
  return l:merged
endfunction

" completefunc entry point.
function! raden#complete(findstart, base) abort
  if a:findstart
    let l:line = getline('.')
    let l:start = col('.') - 1
    while l:start > 0 && l:line[l:start - 1] =~# '\k\|[@:]'
      let l:start -= 1
    endwhile
    return l:start
  endif

  let l:base = a:base

  " Module member completion: 'Base64::<rest>' -> that module's defuns;
  " the member key is the full path up to the last '::' so nested modules
  " (Typecheck::OGTypes::...) resolve correctly.
  if l:base =~# '::'
    let l:parts = split(l:base, '::', v:true)
    if l:parts[-1] ==# ''
      call remove(l:parts, -1)
    endif
    let l:prefix = join(l:parts, '::')
    " rest is whatever follows the final '::'
    let l:rest = substitute(l:base, '^.*::', '', '')
    let l:res = []
    for l:buf in getbufinfo({ 'buflisted': v:true })
      if l:buf.name !~# '\.rdn$'
        continue
      endif
      let l:s = s:symbols(l:buf.bufnr)
      for l:m in get(l:s.members, l:prefix, [])
        if l:rest ==# '' || l:m =~# '^\C' . escape(l:rest, '')
          call add(l:res, s:word(l:prefix . '::' . l:m))
        endif
      endfor
    endfor
    return s:uniq(l:res)
  endif

  let l:candidates = []
  for l:k in s:keywords + s:builtins + s:types
    if l:k =~# '^\C' . escape(l:base, '')
      call add(l:candidates, s:word(l:k))
    endif
  endfor

  let l:merged = s:all_symbols()
  for l:n in l:merged.names
    if l:n =~# '^\C' . escape(l:base, '')
      call add(l:candidates, s:word(l:n))
    endif
  endfor
  for l:t in l:merged.tags
    if l:t =~# '^\C' . escape(l:base, '')
      call add(l:candidates, s:word(l:t))
    endif
  endfor

  return s:uniq(l:candidates)
endfunction

" Remove exact-word duplicates, preserving first appearance (and its kind).
function! s:uniq(list) abort
  let l:seen = {}
  let l:out = []
  for l:item in a:list
    if has_key(l:seen, l:item.word)
      continue
    endif
    let l:seen[l:item.word] = 1
    call add(l:out, l:item)
  endfor
  return l:out
endfunction

" vim: ts=2 sw=2 sts=2 et
