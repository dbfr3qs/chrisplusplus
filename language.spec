# chrisplusplus Language Specification

Comprehensive specification for the chrisplusplus programming language — a strongly-typed, memory-safe, C-like language that compiles to high-performance native binaries via LLVM, with a long-term goal of self-hosting.

---

## 1. Overview

| Property | Value |
|---|---|
| **Name** | chrisplusplus |
| **Paradigm** | Imperative, object-oriented, concurrent |
| **Typing** | Static, strong, inferred where possible |
| **Memory** | Garbage-collected (tracing GC) with `unsafe` manual escape hatch |
| **Concurrency** | async/await with IO/compute distinction |
| **Compilation** | AOT to native binaries via LLVM |
| **Target platforms** | macOS (primary), Linux (secondary), Windows (future) |
| **File extension** | `.chr` |
| **Self-hosting** | Long-term goal: compiler written in chrisplusplus |

### Design Principles
1. **Simplicity** — easy for beginners and AI to learn, generate, and analyze
2. **Safety** — null safety, thread safety, memory safety by default
3. **Performance** — runtime speed prioritized over compile speed
4. **Clarity** — errors, stack traces, and syntax should be human-readable
5. **Pragmatism** — batteries-included stdlib, integrated tooling

---

## 2. Syntax & Structure

### 2.1 General Style
- C-like: curly braces `{}`, semicolons `;`, parentheses for conditions
- Keywords are concise but readable (not overly terse or verbose)
- Entry point is a `main` function

### 2.2 Hello World
```
func main() {
    print("Hello, world!");
}
```

### 2.3 Keywords (preliminary)
```
func, var, let, class, interface, enum, if, else, for, while, return,
import, package, public, private, protected, throw, try, catch, finally,
async, await, io, compute, unsafe, shared, nil, true, false, new, match,
operator
```

### 2.4 Comments
```
// Single-line comment

/*
  Multi-line comment
*/

/// Doc-comment: parsed by the compiler and extractable by tools.
/// Supports @param, @returns, @throws tags.
/// @param a The dividend
/// @param b The divisor
/// @returns The quotient
/// @throws DivisionError if b is zero
func divide(a: Int, b: Int) -> Int { ... }
```

---

## 3. Type System

### 3.1 Primitive Types
| Type | Description |
|---|---|
| `Int` | 64-bit signed integer |
| `Int8`, `Int16`, `Int32` | Sized signed integers |
| `UInt`, `UInt8`, etc. | Unsigned variants |
| `Float` | 64-bit floating point |
| `Float32` | 32-bit floating point |
| `Bool` | Boolean (`true` / `false`) |
| `Char` | Unicode character |
| `String` | UTF-8 string (reference type) |
| `Void` | No return value |

### 3.2 Type Inference
Types are inferred where possible. Explicit annotations are always allowed.
```
var x = 42;           // inferred as Int
var name = "Chris";   // inferred as String
var pi: Float = 3.14; // explicit
```

### 3.3 Null Safety
All types are **non-nullable by default**. Use `?` to opt into nullability.
```
var x: Int = 5;       // cannot be nil — compiler error if nil assigned
var y: Int? = nil;    // explicitly nullable
var z: Int? = 10;     // nullable but currently has a value

// Null-safe access
var len = y?.toString().length;  // returns Int? (nil if y is nil)

// Forced unwrap (throws if nil)
var val = y!;

// Nil coalescing
var safe = y ?? 0;
```

### 3.4 Generics
```
class Box<T> {
    private var value: T;

    func new(value: T) -> Box<T> {
        return Box { value: value };
    }

    func get() -> T {
        return this.value;
    }
}

var intBox = Box.new(42);       // Box<Int>
var strBox = Box.new("hello");  // Box<String>
```

Generic constraints via interfaces:
```
func printAll<T: Printable>(items: List<T>) {
    for item in items {
        print(item.toString());
    }
}
```

### 3.5 Enums
Simple enums and enums with associated values (algebraic data types):
```
// Simple
enum Color {
    Red,
    Green,
    Blue
}

// With associated values
enum Result<T, E> {
    Ok(T),
    Error(E)
}

// Pattern matching with match expressions
var r: Result<Int, String> = Result.Ok(42);
match r {
    Result.Ok(val) => print("Got: ${val}");
    Result.Error(err) => print("Error: ${err}");
}

// Match can also be used as an expression
var message = match r {
    Result.Ok(val) => "Got: ${val}";
    Result.Error(err) => "Error: ${err}";
};
```

---

## 4. Object Model

### 4.1 Classes
```
public class Animal {
    private var name: String;
    protected var age: Int;

    func new(name: String, age: Int) -> Animal {
        return Animal { name: name, age: age };
    }

    public func speak() -> String {
        return "${this.name} makes a sound";
    }
}

public class Dog : Animal {
    private var breed: String;

    func new(name: String, age: Int, breed: String) -> Dog {
        return Dog { name: name, age: age, breed: breed };
    }

    public func speak() -> String {
        return "${this.name} barks!";
    }
}
```

### 4.2 Interfaces
```
interface Serializable {
    func serialize() -> String;
    func deserialize(data: String) -> Self;
}

class User : Serializable {
    public var name: String;

    func serialize() -> String {
        return "{\"name\": \"${this.name}\"}";
    }

    func deserialize(data: String) -> User {
        // parse and return
    }
}
```

### 4.4 Operator Overloading
Classes can define custom operators for readability:
```
class Vector2 {
    public var x: Float;
    public var y: Float;

    func new(x: Float, y: Float) -> Vector2 {
        return Vector2 { x: x, y: y };
    }

    operator +(other: Vector2) -> Vector2 {
        return Vector2.new(this.x + other.x, this.y + other.y);
    }

    operator ==(other: Vector2) -> Bool {
        return this.x == other.x && this.y == other.y;
    }
}

var a = Vector2.new(1.0, 2.0);
var b = Vector2.new(3.0, 4.0);
var c = a + b;  // Vector2 { x: 4.0, y: 6.0 }
```

Overloadable operators: `+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `[]`

### 4.5 Lambdas & Closures
Concise lambda syntax with `=>` and optional type annotations:
```
// Concise — types inferred
var double = (x) => x * 2;
var add = (a, b) => a + b;

// Multi-line
var process = (x: Int) => {
    var result = x * 2;
    return result + 1;
};

// As function arguments
var numbers = [1, 2, 3, 4, 5];
var evens = numbers.filter((x) => x % 2 == 0);
var doubled = numbers.map((x) => x * 2);

// Closures capture variables from enclosing scope
var multiplier = 3;
var tripled = numbers.map((x) => x * multiplier);
```

### 4.3 Access Modifiers
| Modifier | Scope |
|---|---|
| `public` | Accessible from anywhere |
| `private` | Accessible only within the declaring class |
| `protected` | Accessible within the declaring class and subclasses |

Default access (no modifier) is `private`.

---

## 5. Memory Management

### 5.1 Default: Tracing Garbage Collector
- The runtime includes a high-performance tracing GC
- Generational collection for short-lived objects
- Concurrent marking to minimize pause times
- The GC is tunable via runtime flags (heap size, GC frequency, etc.)

### 5.2 Manual Escape Hatch: `unsafe` Blocks
```
unsafe {
    var ptr = alloc<MyStruct>();
    ptr.field = 42;
    free(ptr);
}
```

Inside `unsafe`:
- Raw pointers (`Ptr<T>`) are available
- Manual `alloc<T>()` and `free(ptr)` functions
- Pointer arithmetic allowed
- No GC tracking — developer is fully responsible
- The compiler emits warnings for common mistakes (double-free, use-after-free) where detectable

Outside `unsafe`, pointers and manual allocation are **not available**.

---

## 6. Concurrency & Parallelism

### 6.1 Async/Await with IO/Compute Distinction
Functions declare their concurrency context:

```
// IO-bound: runs on IO thread pool, optimized for waiting
func fetchData(url: String) -> io String {
    var response = await http.get(url);
    return response.body;
}

// CPU-bound: runs on compute thread pool, optimized for throughput
func crunchNumbers(data: List<Int>) -> compute Int {
    var sum = 0;
    for n in data {
        sum = sum + n;
    }
    return sum;
}

// Calling async functions
func main() {
    var data = await fetchData("https://example.com/api");
    var result = await crunchNumbers(parseNumbers(data));
    print(result.toString());
}
```

**Compiler enforcements:**
- Blocking operations inside an `io` context produce a **compiler error**
- CPU-heavy work inside an `io` context produces a **warning**
- The distinction is visible in function signatures — no hidden thread-blocking

### 6.2 Thread-Safe Types with `shared`
```
shared class Counter {
    private var count: Int = 0;

    public func increment() {
        this.count = this.count + 1;  // automatically synchronized
    }

    public func get() -> Int {
        return this.count;
    }
}
```

- `shared` classes have all field access automatically synchronized
- The standard library provides `ConcurrentMap<K, V>`, `ConcurrentList<T>`, `ConcurrentQueue<T>`, etc.
- **The compiler prevents passing non-`shared` mutable types across thread boundaries** — a compile-time error is raised if you try

### 6.3 Channels (optional, for message-passing)
```
var ch = Channel<Int>(bufferSize: 10);

// Producer
async func produce(ch: Channel<Int>) -> io Void {
    for i in 0..100 {
        ch.send(i);
    }
    ch.close();
}

// Consumer
async func consume(ch: Channel<Int>) -> io Void {
    for val in ch {
        print(val.toString());
    }
}
```

---

## 7. Error Handling

### 7.1 Exceptions with Rich Stack Traces
```
func divide(a: Int, b: Int) -> Int {
    if b == 0 {
        throw DivisionError("Cannot divide ${a} by zero");
    }
    return a / b;
}

func main() {
    try {
        var result = divide(10, 0);
    } catch (e: DivisionError) {
        print(e.message);
        print(e.stackTrace);
    } finally {
        print("Done");
    }
}
```

### 7.2 Stack Trace Format
Stack traces include **source file, line number, column, and the actual source line**:
```
DivisionError: Cannot divide 10 by zero

  at divide(a: Int, b: Int) -> Int
     src/math.chr:3:9
     |  throw DivisionError("Cannot divide ${a} by zero");

  at main()
     src/main.chr:8:22
     |  var result = divide(10, 0);
```

### 7.3 Rules
- All exceptions must include a human-readable message (enforced by base `Error` class)
- Chained exceptions preserve the full causal chain via `cause` property
- The compiler **warns** if a function can throw but the caller doesn't handle it (lighter than Java's checked exceptions — it's a warning, not an error)
- Custom exceptions extend the `Error` base class

```
class DivisionError : Error {
    func new(message: String) -> DivisionError {
        return DivisionError { message: message };
    }
}
```

---

## 8. C Interop (FFI)

Seamless foreign function interface for calling C libraries:

```
// Declare external C functions
extern "C" {
    func printf(format: Ptr<Char>, ...) -> Int;
    func malloc(size: UInt) -> Ptr<Void>;
    func free(ptr: Ptr<Void>);
}

// Use them (requires unsafe for pointer operations)
func main() {
    unsafe {
        printf("Hello from C! %d\n", 42);
    }
}
```

- C header files can be imported directly (tooling will auto-generate bindings)
- Platform C libraries (libc, libm, etc.) are accessible out of the box
- Structs can be marked `@CLayout` to match C memory layout for interop

---

## 9. Standard Library (Initial Modules)

Batteries-included but **modular** — only linked modules are included in the binary (tree-shaking).

### Phase 1 (launch)
| Module | Contents |
|---|---|
| `std.core` | Primitives, String, collections (List, Map, Set), math |
| `std.io` | File I/O, stdin/stdout, streams |
| `std.net` | TCP/UDP sockets, DNS resolution |
| `std.http` | HTTP client and server |
| `std.json` | JSON parsing and serialization |
| `std.test` | Test framework (assertions, test runner, mocking) |
| `std.concurrent` | ConcurrentMap, ConcurrentList, ConcurrentQueue, Channel, atomics |

### Phase 2 (future)
| Module | Contents |
|---|---|
| `std.crypto` | Hashing, encryption, TLS |
| `std.cli` | Argument parsing, terminal colors |
| `std.log` | Structured logging |
| `std.db` | Database driver interfaces |
| `std.regex` | Regular expressions |

---

## 10. Tooling

### 10.1 Integrated Build System & Package Manager
```bash
chris new myproject          # Create new project
chris build                  # Compile to native binary
chris run                    # Build and run
chris test                   # Run tests
chris add <package>          # Add dependency
chris fmt                    # Format code (deterministic, single canonical style)
chris lint                   # Lint code
chris build --output json    # Build with machine-parseable error output
```

### 10.2 Project Structure
```
myproject/
├── chris.toml               # Project manifest (deps, metadata)
├── src/
│   └── main.chr             # Entry point
├── test/
│   └── main_test.chr        # Tests
└── build/                   # Compiled output
```

### 10.3 Dependency Manifest (`chris.toml`)
```toml
[package]
name = "myproject"
version = "0.1.0"

[dependencies]
somelib = "1.2.0"
```

---

## 11. Annotations & Reflection

### 11.1 Annotations
```
@Deprecated("Use newMethod instead")
func oldMethod() { ... }

@Serializable
class Config {
    public var host: String;
    public var port: Int;
}

@CLayout
class CStruct {
    public var x: Int32;
    public var y: Int32;
}
```

### 11.2 Reflection
Basic runtime type inspection:
```
var t = typeof(myObject);
print(t.name);          // "User"
print(t.fields);        // ["name", "age"]
print(t.implements);    // ["Serializable"]
```

---

## 12. AI Friendliness

Design choices that optimize for AI code generation and analysis:

1. **Consistent syntax** — no special cases, no context-dependent parsing. One canonical way to express each concept.
2. **Explicit semantics** — `io`/`compute` annotations, `shared` keyword, `?` for nullability make intent visible in code without needing broader context.
3. **Formal grammar** — the language will ship with a complete, unambiguous **EBNF grammar** as a deliverable. This can be embedded in AI system prompts, used for fine-tuning, or consumed by validation tools.
4. **Predictable patterns** — one obvious way to do things (avoid multiple equivalent syntaxes).
5. **Rich type information** — strong types + inference means AI can reason about code without running it.
6. **No implicit type conversions** — all type conversions must be explicit (e.g., `myInt.toFloat()`). This eliminates the "compiles but does something unexpected" failure mode that plagues AI-generated code in languages like JavaScript or C.
7. **Structured compiler errors** — compiler errors are available in a **machine-parseable JSON format** (via `chris build --output json`), enabling AI tools to programmatically read errors and self-correct:
    ```json
    {
      "code": "E0042",
      "severity": "error",
      "message": "Type 'String' is not assignable to 'Int'",
      "file": "src/main.chr",
      "line": 12,
      "column": 5,
      "source_line": "var x: Int = \"hello\";",
      "suggestion": "Change type to 'String' or convert the value"
    }
    ```
8. **Deterministic formatting** — `chris fmt` enforces a single canonical code style (like Go's `gofmt`). AI never has to guess about whitespace, brace placement, or style preferences.
9. **Doc-comments as first-class syntax** — `///` doc-comments are parsed by the compiler and extractable by tools, giving AI structured access to function contracts:
    ```
    /// Divides two integers.
    /// Throws DivisionError if b is zero.
    func divide(a: Int, b: Int) -> Int { ... }
    ```
10. **Self-describing code** — annotations, type signatures, `io`/`compute` markers, and doc-comments together mean most code is understandable without reading the implementation.
11. **Exhaustive match** — `match` expressions require all cases to be handled. The compiler rejects incomplete matches, catching AI mistakes at compile time.
12. **Restricted operator overloading** — only a fixed set of operators can be overloaded, so AI can reason about what `+` means without checking every class definition.

---

## 13. Compiler Architecture

### 13.1 Pipeline
```
Source (.chr) → Lexer → Parser → AST → Type Checker → IR → LLVM IR → Native Binary
```

### 13.2 Implementation Plan
1. **Bootstrap compiler** written in C++ (best LLVM integration, mature tooling)
2. **Self-hosting milestone** — rewrite the compiler in chrisplusplus once the language is mature enough
3. **LLVM backend** for code generation and optimization

### 13.3 Compilation Targets
| Platform | Architecture | Priority |
|---|---|---|
| macOS | ARM64 (Apple Silicon) | P0 — first target |
| macOS | x86_64 | P1 |
| Linux | x86_64 | P1 |
| Linux | ARM64 | P2 |
| Windows | x86_64 | P3 — long-term |

---

## 14. File Extension

The file extension is **`.chr`**.

---

## 15. Example: Complete Program

```
import std.http;
import std.json;

class ApiResponse {
    public var status: Int;
    public var data: String;
}

func fetchUser(id: Int) -> io ApiResponse {
    var url = "https://api.example.com/users/${id}";
    var response = await http.get(url);

    if response.statusCode != 200 {
        throw HttpError("Failed to fetch user: ${response.statusCode}");
    }

    var body = json.parse<ApiResponse>(response.body);
    return body;
}

func main() {
    try {
        var user = await fetchUser(1);
        print("Got user: ${user.data}");
    } catch (e: HttpError) {
        print("HTTP error: ${e.message}");
    } catch (e: JsonError) {
        print("Parse error: ${e.message}");
    }
}
```

---

## 16. Resolved Decisions

| Decision | Choice |
|---|---|
| **File extension** | `.chr` |
| **Constructor syntax** | `func new(...)` |
| **Operator overloading** | Yes — restricted set of overloadable operators |
| **String interpolation** | `"Hello, ${name}"` |
| **Pattern matching** | Full `match` expressions (usable as statements and expressions) |
| **Lambda/closure syntax** | Concise: `(x) => x * 2` |
| **Bootstrap language** | C++ |
