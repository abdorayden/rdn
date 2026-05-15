if exists("b:did_indent")
  finish
endif

let b:did_indent = 1

setlocal indentexpr=GetRadenIndent(v:lnum)
setlocal indentkeys=o,O,0=end,0=else

function! GetRadenIndent(lnum) abort
  let l:line = getline(a:lnum)
  let l:prevlnum = prevnonblank(a:lnum - 1)

  if l:prevlnum == 0
    return 0
  endif

  let l:prev = getline(l:prevlnum)
  let l:indent = indent(l:prevlnum)

  if l:line =~# '^\s*\%(else\|end\)\>'
    let l:indent -= shiftwidth()
  endif

  if l:prev =~# '\<\%(if\|loop\)\>\s*$'
    let l:indent += shiftwidth()
  elseif l:prev =~# '^\s*else\>'
    let l:indent += shiftwidth()
  endif

  return max([l:indent, 0])
endfunction

let b:undo_indent = "setlocal indentexpr< indentkeys<"
