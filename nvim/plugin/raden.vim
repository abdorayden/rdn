" ============================================================================
" plugin/raden.vim  —  Raden nvim plugin (global defaults)
"
" Author: rayden
"
" This file sets the *global* option defaults and most plugin gates.  All
" buffer-local behaviour (the :Raden* commands, tagfunc/completefunc, folding,
" and the opt-in mappings) is wired up by ftplugin/raden.vim, because
" buffer-local commands must be (re)defined for every raden buffer.
"
" If &rtp already points at the nvim/ dir the plugin loads once; if another
" copy is later appended, g:raden_loaded stops a double-run of these defaults.
" ============================================================================

if exists('g:raden_loaded_plugin')
  finish
endif
let g:raden_loaded_plugin = 1

" Enable folding for raden buffers (overridable before this file runs).
if !exists('g:raden_enable_fold')
  let g:raden_enable_fold = 0
endif

" Opt-in <localleader>r / <localleader>e mappings (off by default so the
" plugin never tramples on user maps).
if !exists('g:raden_mappings')
  let g:raden_mappings = 0
endif

" vim: ts=2 sw=2 sts=2 et