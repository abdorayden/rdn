" ============================================================================
" ftplugin/raden.vim  —  per-buffer settings, commands and mappings
"
" Author: rayden
"
" Loaded once per buffer whose filetype resolves to `raden` (autoload through
" ftdetect/raden.vim).  Buffers get:
"   editing help  - commentstring, comments, suffixesadd, formatoptions
"   commands      - :RadenRun, :RadenEval, :RadenTags
"   integration   - tagfunc / completefunc, optional folding
"   mappings      - <localleader>r and <localleader>e, only when opt-in
" ============================================================================

" -- basic local options -----------------------------------------------------
setlocal commentstring=[*\ %s\ *]
setlocal comments=s1:[*,mb:*,ex:*]
setlocal suffixesadd=.rdn
setlocal formatoptions-=t
setlocal formatoptions+=croql
setlocal iskeyword+=_

" Local errorformat so :RadenRun/:RadenEval (via the quickfix) and an
" optional `:compiler raden` all agree on rdn's diagnostic lines.  The
" catch-all %-G%.%# drops the source line and its `^^^` caret context.
" Note: `+=` already inserts the separator comma, so the base must not end
" and the appended part must not start with one.
setlocal errorformat=%f:%l:%c:\ error:\ %m
setlocal errorformat+=%f:%l:%c:\ note:\ %m,%-G%.%#

" -- commands ----------------------------------------------------------------
" :RadenRun [args]  - run the whole buffer (args are passed to rdn verbatim)
" :RadenEval        - run the given range; in visual mode that is the
"                     selection, otherwise defaults to the whole file
" :RadenTags        - list all tags found across open *.rdn buffers
command! -buffer -nargs=* -complete=file RadenRun call raden#run(<q-args>)
command! -buffer -range=% -nargs=0 RadenEval call raden#eval(<line1>, <line2>)
command! -buffer -nargs=0 RadenTags call s:raden_tags()

" -- completion & tags -------------------------------------------------------
setlocal tagfunc=raden#tagfunc
setlocal completefunc=raden#complete

" -- mappings ---------------------------------------------------------------
" Off unless g:raden_mappings is truthy.  <localleader>r runs the file,
" <localleader>e evaluates the visual selection (or the whole file).
if get(g:, 'raden_mappings', 0)
  nnoremap <buffer> <localleader>r :RadenRun<CR>
  vnoremap <buffer> <localleader>e :RadenEval<CR>
  nnoremap <buffer> <localleader>e :%RadenEval<CR>
endif

" :RadenTags helper: output straight to a scratch buffer.
function! s:raden_tags() abort
  let l:lines = raden#tags()
  if empty(l:lines)
    call raden#notify('no tags found in open raden buffers')
    return
  endif
  call s:scratch(l:lines)
endfunction

function! s:scratch(lines) abort
  let l:name = '[Raden Tags]'
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

" -- undo hook ---------------------------------------------------------------
" Restore every option, command and mapping this plugin touched.
let s:undo_argh = 'setlocal commentstring< comments< suffixesadd< formatoptions< iskeyword< tagfunc< completefunc< errorformat<'
let b:undo_ftplugin = s:undo_argh
      \ . ' | delcommand RadenRun | delcommand RadenEval | delcommand RadenTags'
if get(g:, 'raden_mappings', 0)
  let b:undo_ftplugin .=
        \ ' | silent! nunmap <buffer> <localleader>r | silent! nunmap <buffer> <localleader>e'
endif

" vim: ts=2 sw=2 sts=2 et
