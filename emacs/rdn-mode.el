;;; rdn-mode.el --- Major mode for editing Raden source code -*- lexical-binding: t -*-

;; Copyright (c) 2023-2026 Ray Den
;;
;; Permission is hereby granted, free of charge, to any person obtaining a copy
;; of this software and associated documentation files (the "Software"), to deal
;; in the Software without restriction, including without limitation the rights
;; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
;; copies of the Software, and to permit persons to whom the Software is
;; furnished to do so, subject to the following conditions:
;;
;; The above copyright notice and this permission notice shall be included in
;; all copies or substantial portions of the Software.
;;
;; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
;; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
;; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
;; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
;; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
;; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
;; THE SOFTWARE.

;; Author: Rayden
;; URL: https://github.com/abdorayden/rdn

(defconst rdn-mode-syntax-table
  (with-syntax-table (copy-syntax-table)
    (modify-syntax-entry ?\[ ". 1n")
    (modify-syntax-entry ?* ". 23n")
    (modify-syntax-entry ?\] ". 4n")
    (syntax-table))
  "Syntax table for `rdn-mode'.")

(eval-and-compile
  (defconst rdn-keywords
    '("if" "else" "end" "loop" "break" "continue"
      "defun" "apply" "call" "pcall"
      "let" "set" "const" "unlet"
      "load" "loadnative" "add_load_path" "add_native_path"
      "enum" "reset")
    "Raden language keywords.")

  (defconst rdn-builtins
    '("print" "pop" "dup" "swap" "type" "to_string" "exit"
      "append" "remove" "index" "len" "error"
      "+" "-" "*" "/" "=" "!=" "<" ">" "<=" ">="
      "!" "|" "&" "^" "<<" ">>")
    "Raden builtin words and operators.")

  (defconst rdn-constants
    '("true" "false" "null")
    "Raden literal constants."))

(defconst rdn-highlights
  `((,(regexp-opt rdn-keywords 'symbols) . font-lock-keyword-face)
    (,(regexp-opt rdn-builtins 'symbols) . font-lock-builtin-face)
    (,(regexp-opt rdn-constants 'symbols) . font-lock-constant-face))
  "Font-lock rules for `rdn-mode'.")

;;;autoload
(define-derived-mode rdn-mode prog-mode "rdn"
  "Major mode for editing Raden source code."
  :syntax-table rdn-mode-syntax-table
  (setq font-lock-defaults '(rdn-highlights))
  (setq-local comment-start "[* ")
  (setq-local comment-end " *]"))

(add-to-list 'auto-mode-alist '("\\.rdn\\'" . rdn-mode))

(provide 'rdn-mode)
