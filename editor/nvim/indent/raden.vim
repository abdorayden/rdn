" ============================================================================
" indent/raden.vim  —  block-aware indentation for Raden
"
" Author: rayden
"
" Raden is postfix, so blocks "open" on the keyword that *ends* a line
" (if / loop / defun / module / apply) and close with `end`; `else` restarts
" an if-body at the same depth.  The rules implemented here:
"   - a line after an opener is indented one shiftwidth deeper
"   - `end` / `else` lines are dedented by one level per closer token
"   - a statement following an `end` inherits that end's (outer) depth
"   - continuations of an unfinished list literal align to the opening `(`
"
" Comments ([*...*], possibly spanning lines) and string literals are blanked
" via raden#striplines() before any keyword look-up, so they never skew the
" nesting.
" ============================================================================

if exists('b:did_indent')
  finish
endif
let b:did_indent = 1

setlocal indentexpr=RadenIndent(v:lnum)
setlocal indentkeys=o,O,0=end,0=else
setlocal autoindent

function! RadenIndent(lnum) abort
  let l:lines = raden#striplines()

  " The line whose indentation we are computing, stripped of noise.
  let l:line = l:lines[a:lnum - 1]

  " Nearest previous content line (comments/blank lines are invisible after
  " stripping, so they are skipped automatically).
  let l:prev = s:prev_content(a:lnum - 1, l:lines)
  if l:prev == 0
    return 0
  endif
  let l:prevline = l:lines[l:prev - 1]

  " Base depth: the previous content line's own indentation, adjusted for the
  " block transition it represents.
  let l:ind = indent(l:prev)
  if raden#is_block_open(l:prevline)
    " opener on the previous line -> its body indents one level
    let l:ind += shiftwidth()
  elseif l:prevline =~# '^\s*else\>'
    " an else restarts a body at the same depth as the if's
    let l:ind += shiftwidth()
  endif

  " Closers on *this* line dedent by one level each (end end end).
  if raden#is_block_close(l:line)
    let l:ind -= raden#closers_count(l:line) * shiftwidth()
    return max([l:ind, 0])
  endif

  " Continuation of an unfinished list literal: align under its opening '('.
  let l:open = s:last_open(1, a:lnum - 1, l:lines)
  if l:open > 0
    let l:paren = strridx(l:lines[l:open - 1], '(')
    if l:paren >= 0 && l:paren > l:ind
      let l:ind = l:paren
    endif
  endif

  return max([l:ind, 0])
endfunction

" Column of the last unmatched '(' across lines 1..end; 0 when balanced.
" Runs the same span-free stripped text as everything else here.
function! s:last_open(start, end, lines) abort
  let l:depth = 0
  let l:last = 0
  for l:i in range(a:start, a:end)
    let l:sl = a:lines[l:i - 1]
    let l:o = count(l:sl, '(')
    let l:c = count(l:sl, ')')
    if l:o + l:c > 0
      let l:last = l:i
    endif
    let l:depth += l:o - l:c
  endfor
  return l:depth > 0 ? l:last : 0
endfunction

" Nearest line index <= n with any content after stripping.
function! s:prev_content(n, lines) abort
  let l:i = a:n
  while l:i > 0
    if a:lines[l:i - 1] =~# '\S'
      return l:i
    endif
    let l:i -= 1
  endwhile
  return 0
endfunction

let b:undo_indent = "setlocal indentexpr< indentkeys< autoindent<"

" vim: ts=2 sw=2 sts=2 et