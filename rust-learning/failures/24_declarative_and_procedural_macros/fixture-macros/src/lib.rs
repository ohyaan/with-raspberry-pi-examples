use proc_macro::{TokenStream, TokenTree};

// This derive intentionally accepts only: struct Name;
#[proc_macro_derive(FixtureName)]
pub fn fixture_name(input: TokenStream) -> TokenStream {
    let tokens: Vec<_> = input.into_iter().collect();
    match tokens.as_slice() {
        [
            TokenTree::Ident(keyword),
            TokenTree::Ident(name),
            TokenTree::Punct(end),
        ] if keyword.to_string() == "struct" && end.as_char() == ';' => format!(
            "impl FixtureName for {name} {{
                    fn fixture_name() -> &'static str {{ stringify!({name}) }}
                }}"
        )
        .parse()
        .expect("fixed implementation syntax"),
        _ => "compile_error!(\"FixtureName accepts only a private unit struct\");"
            .parse()
            .expect("fixed diagnostic syntax"),
    }
}

// Attribute output replaces the input item, so retain that item explicitly.
#[proc_macro_attribute]
pub fn add_debug(arguments: TokenStream, item: TokenStream) -> TokenStream {
    if !arguments.is_empty() {
        return "compile_error!(\"add_debug takes no arguments\");"
            .parse()
            .expect("fixed diagnostic syntax");
    }
    let mut output: TokenStream = "#[derive(Debug)]".parse().expect("fixed attribute syntax");
    output.extend(item);
    output
}

#[proc_macro]
pub fn fixture_value(input: TokenStream) -> TokenStream {
    if input.is_empty() {
        "46_700i32".parse().expect("fixed expression syntax")
    } else {
        "compile_error!(\"fixture_value takes no arguments\")"
            .parse()
            .expect("fixed diagnostic syntax")
    }
}
