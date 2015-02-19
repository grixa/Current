# Bricks

![](https://raw.githubusercontent.com/KnowSheet/Bricks/master/holy_bricks.jpg)

Our own core pieces to reuse.

# Documentation

<sub>The following documentation has been auto-generated from source code by means of a `TODO(dkorolev)` script. Do not edit this file.</sub>

## Cerealize

Bricks uses the **Cereal** library for JSON and Binary serialization of C++ objects:
[Cereal Website](http://uscilab.github.io/cereal/), [Cereal GitHub](http://uscilab.github.io/cereal/).

Use `#include "Bricks/cerealize/cerealize.h"` and `using namespace bricks::cerealize;` to run the code snippets below.
```cpp
// Add a `serialize()` method to make a C++ structure "cerealizable".
struct SimpleType {
  int number;
  std::string string;
  std::vector<int> vector_int;
  std::map<int, std::string> map_int_string;
  
  template <typename A> void serialize(A& ar) {
    // Use `CEREAL_NVP(member)` to keep member names when using JSON.
    ar(CEREAL_NVP(number),
       CEREAL_NVP(string),
       CEREAL_NVP(vector_int),
       CEREAL_NVP(map_int_string));
  }
};
```
```cpp
// Use `JSON()` and `JSONParse()` to create and parse JSON-s.
SimpleType x;
x.number = 42;
x.string = "test passed";
x.vector_int.push_back(1);
x.vector_int.push_back(2);
x.vector_int.push_back(3);
x.map_int_string[1] = "one";
x.map_int_string[42] = "the question";

// `JSON(object)` converts a cerealize-able object into a JSON string.
const std::string json = JSON(x);

// `JSONParse<T>(json)` creates an instance of T from a JSON.
const SimpleType y = JSONParse<SimpleType>(json);

// `JSONParse(json, T& out)` allows omitting the type.
SimpleType z;
JSONParse(json, z);
```
```cpp
// Use `load()/save()` instead of `serialize()` to customize serialization.
struct LoadSaveType {
  int a;
  int b;
  int sum;
  
  template <typename A> void save(A& ar) const {
    ar(CEREAL_NVP(a), CEREAL_NVP(b));
  }

  template <typename A> void load(A& ar) {
    ar(CEREAL_NVP(a), CEREAL_NVP(b));
    sum = a + b;
  }
};

LoadSaveType x;
x.a = 2;
x.b = 3;
EXPECT_EQ(5, JSONParse<LoadSaveType>(JSON(x)).sum);
```
```cpp
// Polymorphic types are supported with some caution.
struct ExamplePolymorphicType {
  std::string base;
  explicit ExamplePolymorphicType(const std::string& base = "") : base(base) {}

  virtual std::string AsString() const = 0;
  
  template <typename A> void serialize(A& ar) const {
    ar(CEREAL_NVP(base));
  }
};

struct ExamplePolymorphicInt : ExamplePolymorphicType {
  int i;
  explicit ExamplePolymorphicInt(int i = 0)
      : ExamplePolymorphicType("int"), i(i) {}

  virtual std::string AsString() const override {
    return Printf("%s, %d", base.c_str(), i);
  }
  
  template <typename A> void serialize(A& ar) const {
    ExamplePolymorphicType::serialize(ar);
    ar(CEREAL_NVP(i));
  }
};
// Need to register the derived type.
CEREAL_REGISTER_TYPE(ExamplePolymorphicInt);

struct ExamplePolymorphicDouble : ExamplePolymorphicType {
  double d;
  explicit ExamplePolymorphicDouble(double d = 0)
      : ExamplePolymorphicType("double"), d(d) {}

  virtual std::string AsString() const override {
    return Printf("%s, %lf", base.c_str(), d);
  }
  
  template <typename A> void serialize(A& ar) const {
    ExamplePolymorphicType::serialize(ar);
    ar(CEREAL_NVP(d));
  }
};
// Need to register the derived type.
CEREAL_REGISTER_TYPE(ExamplePolymorphicDouble);

const std::string json_int =
  JSON(WithBaseType<ExamplePolymorphicType>(ExamplePolymorphicInt(42)));

const std::string json_double =
  JSON(WithBaseType<ExamplePolymorphicType>(ExamplePolymorphicDouble(M_PI)));

EXPECT_EQ("int, 42",
          JSONParse<std::unique_ptr<ExamplePolymorphicType>>(json_int)->AsString());
  
EXPECT_EQ("double, 3.141593",
          JSONParse<std::unique_ptr<ExamplePolymorphicType>>(json_double)->AsString());
```
## Run-time Type Dispatching

Bricks can dispatch calls to the right implementation at runtime, with user code being free of virtual functions.

This comes especially handy when processing log entries from a large stream of data, where only a few types are of immediate interest.

Use `#include "Bricks/rtti/dispatcher.h"` and `using namespace bricks::rtti;` to run the code snippets below.

`TODO(dkorolev)` a wiser way for the end user to leverage the above is by means of `Sherlock` once it's checked in.
```cpp
struct ExampleBase {
  virtual ~ExampleBase() = default;
};

struct ExampleInt : ExampleBase {
  int i;
  explicit ExampleInt(int i) : i(i) {}
};

struct ExampleString : ExampleBase {
  std::string s;
  explicit ExampleString(const std::string& s) : s(s) {}
};

struct ExampleMoo : ExampleBase {
};

struct ExampleProcessor {
  std::string result;
  void operator()(const ExampleBase&) { result = "unknown"; }
  void operator()(const ExampleInt& x) { result = Printf("int %d", x.i); }
  void operator()(const ExampleString& x) { result = Printf("string '%s'", x.s.c_str()); }
  void operator()(const ExampleMoo&) { result = "moo!"; }
};
  
typedef RuntimeTupleDispatcher<ExampleBase,
                               tuple<ExampleInt, ExampleString, ExampleMoo>> Dispatcher;

ExampleProcessor processor;

Dispatcher::DispatchCall(ExampleBase(), processor);
EXPECT_EQ(processor.result, "unknown");
  
Dispatcher::DispatchCall(ExampleInt(42), processor);
EXPECT_EQ(processor.result, "int 42");
  
Dispatcher::DispatchCall(ExampleString("foo"), processor);
EXPECT_EQ(processor.result, "string 'foo'");
  
Dispatcher::DispatchCall(ExampleMoo(), processor);
EXPECT_EQ(processor.result, "moo!");
```
## HTTP

Use `#include "Bricks/net/api/api.h"` and `using namespace bricks::net::api;` to run the code snippets below.

### Client

```cpp
// Simple GET.
EXPECT_EQ("OK", HTTP(GET("test.tailproduce.org/ok")).body);

// More fields.
const auto response = HTTP(GET("test.tailproduce.org/ok"));
EXPECT_EQ(200, static_cast<int>(response.code));
EXPECT_EQ("OK", response.body);
```
```cpp
// POST is supported as well.
EXPECT_EQ("OK", HTTP(POST("test.tailproduce.org/ok"), "BODY", "text/plain").body);
  
// Beyond plain strings, cerealizable objects can be passed in.
// JSON will be sent, as "application/json" content type.
EXPECT_EQ("OK", HTTP(POST("test.tailproduce.org/ok"), SimpleType()).body);

```
HTTP client supports headers, POST-ing data to and from files, and many other features as well. Check the unit test in `bricks/net/api/test.cc` for more details.
### Server
```cpp
// Simple "OK" endpoint.
HTTP(port).Register("/ok", [](Request r) {
  r("OK");
});
```
```cpp
// Accessing input fields.
HTTP(port).Register("/demo", [](Request r) {
  // TODO(dkorolev): `r.method`.
  // TODO(dkorolev): `r.body`.
  r(r.url.query["q"] + ' ' + r.http.Method() + ' ' + r.http.Body());
});

```
```cpp
// Constructing a more complex response.
HTTP(port).Register("/found", [](Request r) {
  r("Yes.",
    HTTPResponseCode::Accepted,            // TODO(dkorolev): Dot notation.
    "text/html",
    HTTPHeaders({{"custom", "header"}}));  // TODO(dkorolev): Dot notation.
});
```
```cpp
// An input record that would be passed in as a JSON.
struct PennyInput {
  std::string op;
  std::vector<int> x;
  template <typename A> void serialize(A& ar) {
    ar(CEREAL_NVP(op), CEREAL_NVP(x));
  }   
  // TODO(dkorolev): Safe mode wrt parsing malformed JSON.
};

// An output record that would be sent back as a JSON.
struct PennyOutput {
  int result;
  template <typename A> void serialize(A& ar) {
    ar(CEREAL_NVP(result));
  }   
};  

// Doing Penny-level arithmetics for fun and performance testing.
HTTP(port).Register("/penny", [](Request r) {
  // TODO(dkorolev): `r.body`, and its safe mode.
  const auto input = JSONParse<PennyInput>(r.http.Body());
  int result = 0;
  if (input.op == "add") {
    for (const auto v : input.x) {
      result += v;
    }
  } else if (input.op == "mul") {
    result = 1;
    for (const auto v : input.x) {
      result *= v;
    }
  }
  r(PennyOutput{result});
});
```
HTTP server also has support for several other features, check out the `bricks/net/api/test.cc` unit test.

**TODO(dkorolev)**: Chunked response example, with a note that it goes to Sherlock.
## Extras

Other useful bits include chart visualization, file system, string and system clock utilities.
