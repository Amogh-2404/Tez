# Contributing to Tez

First off, thank you for considering contributing to Tez! It's people like you that make Tez a great educational resource and tool for the embedded systems community.

## Code of Conduct

This project and everyone participating in it is governed by the [Tez Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code. Please report unacceptable behavior to ramogh2404@gmail.com.

## How Can I Contribute?

### Reporting Bugs

This section guides you through submitting a bug report for Tez. Following these guidelines helps maintainers and the community understand your report, reproduce the behavior, and find related reports.

**Before Submitting A Bug Report:**
- Check the [documentation](README.md) for common issues
- Search [existing issues](../../issues) to see if the problem has already been reported
- Perform a basic troubleshooting check (rebuild, clear caches, etc.)

**How Do I Submit A Good Bug Report?**

Bugs are tracked as [GitHub issues](../../issues). Create an issue and provide the following information:

- **Use a clear and descriptive title** for the issue
- **Describe the exact steps to reproduce the problem** in as much detail as possible
- **Provide specific examples** to demonstrate the steps
- **Describe the behavior you observed** and what behavior you expected to see
- **Include logs or error messages** if applicable
- **Specify your environment:**
  - OS and version
  - Compiler and version
  - Boost version
  - How you built Tez (CMake options, etc.)

### Suggesting Enhancements

This section guides you through submitting an enhancement suggestion for Tez.

**Before Submitting An Enhancement Suggestion:**
- Check if the enhancement is already in the [roadmap](README.md#roadmap)
- Search [existing issues](../../issues) to see if the enhancement has already been suggested

**How Do I Submit A Good Enhancement Suggestion?**

Enhancement suggestions are tracked as [GitHub issues](../../issues). Create an issue and provide the following information:

- **Use a clear and descriptive title**
- **Provide a detailed description** of the suggested enhancement
- **Explain why this enhancement would be useful** to most Tez users
- **List any alternatives** you've considered
- **Provide examples** of how the enhancement would work

### Your First Code Contribution

Unsure where to begin? You can start by looking through issues tagged with:
- `good first issue` - Issues which should only require a few lines of code
- `help wanted` - Issues which need community assistance

### Pull Requests

The process described here has several goals:
- Maintain Tez's quality
- Fix problems that are important to users
- Engage the community in working toward the best possible Tez
- Enable a sustainable system for Tez's maintainers to review contributions

**Pull Request Process:**

1. **Fork the repo** and create your branch from `main`
   ```bash
   git checkout -b feature/my-new-feature
   # or
   git checkout -b fix/issue-123
   ```

2. **Follow the coding style** (see Code Style Guide below)

3. **Add tests** for your changes
   - Unit tests go in `tests/`
   - Run existing tests: `cd build && make TezTests && ./TezTests`

4. **Update documentation**
   - Update README.md if you changed functionality
   - Update CLAUDE.md if you changed architecture
   - Add comments to explain complex code

5. **Ensure your code builds without warnings**
   ```bash
   mkdir -p build && cd build
   cmake ..
   make
   ```

6. **Run tests** to ensure nothing breaks
   ```bash
   ./TezTests  # If GTest is installed
   ```

7. **Commit your changes** with a clear message
   ```bash
   git commit -m "Add feature: brief description

   More detailed explanation of what this commit does and why.
   Fixes #123"
   ```

8. **Push to your fork** and submit a pull request

9. **Wait for review** - maintainers will review your PR and may request changes

## Code Style Guide

### C++ Style

- **Use modern C++17** features where appropriate
- **Follow RAII** principles - no manual memory management
- **Use smart pointers** (`std::shared_ptr`, `std::unique_ptr`) over raw pointers
- **Prefer `const`** whenever possible
- **Use meaningful variable names** - clarity over brevity

### Formatting

- **Indentation**: 4 spaces (no tabs)
- **Line length**: Prefer < 100 characters, max 120
- **Braces**: K&R style (opening brace on same line)
  ```cpp
  void function() {
      // code here
  }
  ```
- **Spacing**: Space after keywords, around operators
  ```cpp
  if (condition) {  // Space after 'if', around '=='
      x = y + z;    // Spaces around operators
  }
  ```

**Automatic Formatting:**
```bash
# Install clang-format
brew install clang-format  # macOS
sudo apt-get install clang-format  # Ubuntu

# Format your code
clang-format -i src/*.cpp include/*.hpp
```

### Naming Conventions

- **Functions/Variables**: `snake_case`
  ```cpp
  void handle_request();
  std::string file_path;
  ```
- **Classes/Structs**: `PascalCase`
  ```cpp
  class ThreadPool {};
  struct Response {};
  ```
- **Constants**: `UPPER_SNAKE_CASE`
  ```cpp
  constexpr size_t MAX_CONTENT_LENGTH = 10 * 1024 * 1024;
  ```
- **Private members**: `snake_case_` (trailing underscore)
  ```cpp
  class Foo {
  private:
      int member_variable_;
  };
  ```

### Comments

- **Use `//` for single-line comments**
- **Use `/* */` for multi-line comments**
- **Document public APIs** with brief description
  ```cpp
  // Sanitizes file path to prevent directory traversal
  // Returns empty string if path is invalid
  std::string sanitize_path(const std::string& filename);
  ```
- **Explain "why", not "what"** - code should be self-documenting

### Error Handling

- **Use exceptions** for exceptional conditions
- **Use Boost error codes** for expected errors (network, file I/O)
- **Always validate input** before processing
- **Log errors** with context

Example:
```cpp
boost::system::error_code ec;
read_until(socket, buffer, "\r\n\r\n", ec);
if (ec) {
    std::cerr << "Error reading request: " << ec.message() << "\n";
    return;
}
```

### Thread Safety

- **Always use mutexes** for shared state
- **Prefer `std::lock_guard`** over manual lock/unlock
- **Document thread-safety** assumptions
  ```cpp
  // Thread-safe: Protected by cache_mutex
  Response get_cached_response(const std::string& path);
  ```

## Testing Guidelines

### Writing Tests

- **Test file naming**: `test_<component>.cpp`
- **Test naming**: Descriptive and specific
  ```cpp
  TEST(RouterTest, HealthEndpointReturnsOk) {
      // ...
  }
  ```
- **Test one thing** per test function
- **Use assertions** liberally
- **Test edge cases**: empty strings, null values, max values

### Running Tests

```bash
cd build
make TezTests
./TezTests

# Run specific test
./TezTests --gtest_filter=RouterTest.*
```

## Security Guidelines

Security is critical for Tez. When contributing:

1. **Never introduce vulnerabilities**:
   - No SQL injection (we don't use SQL, but principle applies)
   - No path traversal - always use `sanitize_path()`
   - No buffer overflows - validate sizes before allocation
   - No XSS - sanitize output if rendering HTML

2. **Validate all input**:
   - Check Content-Length before reading body
   - Validate header sizes
   - Sanitize file paths
   - Validate JSON structure

3. **Report security issues privately**:
   - Email security vulnerabilities to ramogh2404@gmail.com
   - Do NOT open public issues for security problems
   - See [SECURITY.md](SECURITY.md) for disclosure policy

## Commit Message Guidelines

Good commit messages help understand project history.

### Format

```
<type>: <short summary>

<optional detailed description>

<optional footer>
```

### Types

- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, no logic change)
- `refactor`: Code refactoring (no functional change)
- `test`: Adding or updating tests
- `chore`: Build process, dependencies, etc.

### Examples

```
feat: Add LRU cache eviction algorithm

Replace clear-all cache eviction with proper LRU implementation
using list + unordered_map for O(1) operations. This prevents
cache poisoning attacks.

Fixes #42
```

```
fix: Validate Content-Length before memory allocation

Add check for MAX_CONTENT_LENGTH before allocating body buffer.
Returns 413 Payload Too Large if request exceeds limit.

Fixes #67
```

## Documentation

When adding new features:

1. **Update README.md** if user-facing
2. **Update CLAUDE.md** if architectural
3. **Update SECURITY.md** if security-related
4. **Add inline comments** for complex logic
5. **Update CHANGELOG.md** for releases

## Questions?

Don't hesitate to ask questions:
- Open a [discussion](../../discussions)
- Create an issue with the `question` label
- Reach out to maintainers

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

## Recognition

Contributors are recognized in:
- GitHub contributors list
- CHANGELOG.md (for significant contributions)
- Special thanks in release notes

Thank you for contributing to Tez! 🚀
