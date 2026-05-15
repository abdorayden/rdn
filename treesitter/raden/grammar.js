module.exports = grammar({
  name: "raden",

  extras: ($) => [/\s+/, $.comment],

  word: ($) => $.identifier,

  rules: {
    source_file: ($) => repeat($._form),

    _form: ($) =>
      choice(
        $.list,
        $.string,
        $.float,
        $.integer,
        $.boolean,
        $.conditional,
        $.loop_control,
        $.binding_keyword,
        $.builtin,
        $.operator,
        $.identifier
      ),

    comment: ($) =>
      seq("[*", repeat(choice(/[^*]+/, /\*+[^]\*]/)), "*]"),

    list: ($) => seq("(", repeat($._form), ")"),

    string: ($) =>
      seq(
        '"',
        repeat(choice(token.immediate(/[^"\\\n]+/), $.escape_sequence)),
        '"'
      ),

    escape_sequence: ($) => token.immediate(seq("\\", /["\\nrt]/)),

    integer: ($) =>
      token(
        choice(
          /[-+]?\d+/,
          /[-+]?0x[0-9A-Fa-f]+/,
          /[-+]?0b[01]+/,
          /[-+]?0o[0-7]+/
        )
      ),

    float: ($) =>
      token(choice(/[-+]?\d+\.\d+([eE][-+]?\d+)?/, /[-+]?\d+[eE][-+]?\d+/)),

    boolean: ($) => choice("true", "false"),

    conditional: ($) => choice("if", "else", "end"),

    loop_control: ($) => choice("loop", "break", "continue"),

    binding_keyword: ($) => choice("let", "const"),

    builtin: ($) =>
      choice(
        "print",
        "type",
        "exit",
        "pop",
        "swap",
        "dup",
        "to_string",
        "append",
        "remove",
        "index",
        "len"
      ),

    operator: ($) =>
      choice(
        "+",
        "-",
        "*",
        "/",
        "<<",
        ">>",
        "<=",
        ">=",
        "!=",
        "=",
        "<",
        ">",
        "|",
        "&",
        "^",
        "!"
      ),

    identifier: ($) => /[A-Za-z_][A-Za-z0-9_]*/,
  },
});
