# Rust learning examples

Exact examples from all 41 lessons of the [Rust course](https://ohyaan.github.io/rust-learning/). Existing article source, multi-file layouts, fixtures and intended failures are reused; this is not a second tutorial series.

Where an article supplies only a main.rs program, a minimal edition-2024 Cargo
manifest is added as packaging. Existing multi-file article manifests are kept
unchanged. Expected output and CLI fixtures come from the same article.

## Get and run one lesson

Use stable Rust with edition 2024 support. The actual verification toolchain is recorded in [VERIFICATION.md](VERIFICATION.md); no untested minimum version is promised. No CMake, electronics or crates.io dependencies are needed. The macro lesson depends only on its local proc-macro package.

The checked outputs target Linux, including the configuration lesson's platform
label. Portability to other operating systems is a separate check, not implied
by passing the Raspberry Pi fixtures.

```bash
git clone https://github.com/ohyaan/with-raspberry-pi-examples.git
cd with-raspberry-pi-examples/rust-learning
cd lessons/01_install_rust_and_first_cargo_project
cargo run --offline
cargo test --offline
```

For an existing checkout, use its rust-learning directory instead of cloning over modified files. Each lesson README gives its own commands and links back to the article. CLI/status lessons use checked-in fixtures and require the documented arguments.

## Layout and workspace boundaries

- `lessons/`: 41 working lesson directories, including multi-file projects.
- `failures/`: separately isolated intentional compile/test failures.
- `tools/`: output, failure and source-equality verification.
- `catalog.json`: ordered lesson metadata, package names and expected arguments.

The category workspace contains 40 lessons. The macros lesson retains its original nested two-package workspace and is checked separately. Intentional failures have isolated workspaces and are excluded from normal builds. This keeps the article manifests unchanged. See [Cargo workspace documentation](https://doc.rust-lang.org/cargo/reference/workspaces.html).

## Check the complete course

From the category directory:

```bash
cargo test --workspace --offline --jobs 2
cargo test --workspace --offline --release --jobs 2
python3 tools/verify.py
python3 tools/verify.py --failures
python3 tools/verify.py --cpp
```

The Python verifier includes the separate macro workspace, debug/release runs, exact expected stdout, fixtures and the feature lesson's enabled/disabled variants. The --failures command additionally checks rejection reasons. --cpp additionally checks the twelve existing C++ comparisons with GCC/G++ and C++20.

Build concurrency is limited to two. Operations have timeouts; no background worker, device access, live sensor read or benchmark is started by the verifier. All temporary compiler artifacts are kept under target/. The programs' own tests use their documented temporary fixtures.

## Keep article and repository sources aligned

```bash
python3 tools/check_sources.py
python3 tools/check_sources.py --site-source /path/to/blog-raspberry-pi-source
```

The first command checks copied-source hashes. The second additionally reconstructs the expected files from the site checkout and compares exact bytes. Explanation fragments are identified as fragments, not silently wrapped or advertised as standalone programs. Edits to an article's source need a corresponding example update.

## Lessons

- [Install Rust on Raspberry Pi and Run Your First Cargo Project](lessons/01_install_rust_and_first_cargo_project/README.md)
- [Rust Variables, Types, Functions and Control Flow](lessons/02_variables_types_functions_and_control_flow/README.md)
- [Rust Types, Operators and Expressions](lessons/03_types_operators_and_expressions/README.md)
- [Rust Ownership, Moves, Copy and Clone](lessons/04_ownership_moves_copy_and_clone/README.md)
- [Rust References, Borrowing and Slices](lessons/05_references_borrowing_and_slices/README.md)
- [Rust Structs, Methods and Associated Functions](lessons/06_structs_methods_and_associated_functions/README.md)
- [Rust Enums, Match and Patterns](lessons/07_enums_match_and_patterns/README.md)
- [Rust Strings, Vectors and Hash Maps](lessons/08_strings_vectors_and_hash_maps/README.md)
- [Rust Option, Result and Error Handling](lessons/09_option_result_and_error_handling/README.md)
- [Rust Crates, Modules, Paths and Visibility](lessons/10_crates_modules_paths_and_visibility/README.md)
- [Rust Generics, Bounds and Const Generics](lessons/11_generics_bounds_and_const_generics/README.md)
- [Rust Traits, Implementations and Associated Types](lessons/12_traits_implementations_and_associated_types/README.md)
- [Rust Lifetimes and Borrowed APIs](lessons/13_lifetimes_and_borrowed_apis/README.md)
- [Rust Trait Objects and Dispatch: dyn Trait vs impl Trait](lessons/14_trait_objects_and_dispatch/README.md)
- [Rust Closures and Function Pointers](lessons/15_closures_and_function_pointers/README.md)
- [Rust Iterators and Lazy Adapters](lessons/16_iterators_and_lazy_adapters/README.md)
- [Rust Tests, Documentation and Cargo](lessons/17_tests_documentation_and_cargo/README.md)
- [Rust Box, Deref and Drop](lessons/18_box_deref_and_drop/README.md)
- [Rust Rc, Weak, Cell and RefCell](lessons/19_rc_weak_cell_and_refcell/README.md)
- [Rust Threads, Send, Sync, Arc and Locks](lessons/20_threads_send_sync_arc_and_locks/README.md)
- [Rust Channels, Atomics and Worker Shutdown](lessons/21_channels_atomics_and_shutdown/README.md)
- [Rust async, await and Future](lessons/22_async_await_and_future/README.md)
- [Rust Pin, Unpin and Async Boundaries](lessons/23_pin_unpin_and_async_boundaries/README.md)
- [Rust Declarative and Procedural Macros](lessons/24_declarative_and_procedural_macros/README.md)
- [Rust Attributes, cfg, Features and Editions](lessons/25_attributes_cfg_features_and_editions/README.md)
- [Rust Unsafe, Raw Pointers and FFI](lessons/26_unsafe_raw_pointers_and_ffi/README.md)
- [Rust Files, Arguments and a Linux CLI](lessons/27_files_arguments_and_linux_cli/README.md)
- [Rust Raspberry Pi System-Status Project](lessons/28_raspberry_pi_system_status_project/README.md)
- [Reading the Rust Reference](lessons/29_reading_the_rust_reference/README.md)
- [Rust vs C++: Values, Ownership and RAII](lessons/30_rust_cpp_values_ownership_and_raii/README.md)
- [Rust Borrowing vs C++ const: References and Lifetimes](lessons/31_rust_cpp_borrowing_const_and_lifetimes/README.md)
- [Rust Traits vs C++ Interfaces: Generics and Dispatch](lessons/32_rust_cpp_traits_generics_and_dispatch/README.md)
- [Rust Result and C++ Exceptions: Error Contracts](lessons/33_rust_cpp_errors_exceptions_and_contracts/README.md)
- [Rust Moves vs C++ Move Semantics](lessons/34_rust_moves_vs_cpp_move_semantics/README.md)
- [Rust Pin and C++ Address Stability](lessons/35_rust_cpp_address_stability_and_pin/README.md)
- [Strategy in Rust: Functions, Closures and Traits](lessons/36_rust_strategy_functions_traits_and_dispatch/README.md)
- [State in Rust: Enums and Typestate](lessons/37_rust_state_enums_and_typestate/README.md)
- [Factory and Builder in Rust: Validated Construction](lessons/38_rust_factory_builder_and_validated_construction/README.md)
- [Visitor in Rust: Enums and Open Behaviour](lessons/39_rust_visitor_enums_and_open_behaviour/README.md)
- [Observer in Rust: Events and Shutdown](lessons/40_rust_observer_events_and_shutdown/README.md)
- [Rust Ownership, IDs and ECS Design](lessons/41_rust_ownership_ids_and_ecs_design/README.md)

Passing these fixtures is evidence for these programs, not proof of every possible input or every target. No performance or hardware validation is implied.
