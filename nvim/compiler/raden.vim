" ============================================================================
" compiler/raden.vim  —  Raden compile/error support for :make
"
" Author: rayden
"
" Gives the Raden interpreter a compiler front-end:
"   :compiler raden
"   :make            runs `rdn %` and fills the quickfix list from stderr
"
" rdn's diagnostic lines look exactly like
"   /path/to/file.rdn:3:7: error: + requires known variable '@'
" (see the notes in doc/raden.txt for the full grammar).  The two formats
" below pick those up; the %-G%.%# catch-all swallows the source lines and
" the `^^^` caret lines that rdn echoes underneath, which are noise here.
" ============================================================================

if exists('current_compiler')
  finish
endif
let current_compiler = 'raden'

" Honour an explicit interpreter first; otherwise fall back to detection
" (so a repo-local ./main works without installing rdn).
let s:exec = get(g:, 'raden_exec', '')
if empty(s:exec)
  let s:exec = raden#exec()
endif
if empty(s:exec)
  let s:exec = 'rdn'
endif

" makeprg is a shell command line; assigned via &l: so no Ex-line parsing can
" mangle the space or the literal ' %' file placeholder.
let s:val = shellescape(s:exec) . ' %'
let &l:makeprg = s:val
unlet s:exec s:val

CompilerSet errorformat=
      \%f:%l:%c:\ error:\ %m,
      \%f:%l:%c:\ note:\ %m,
      \%-G%.%#

" vim: ts=2 sw=2 sts=2 et

" vim: ts=2 sw=2 sts=2 et