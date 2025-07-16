# Loguru Usage Guide

[Loguru](https://github.com/Delgan/loguru) is a Python logging library that makes logging both powerful and pleasant to use.

## Basic Usage

```python
from loguru import logger

logger.debug("This is a debug message")
logger.info("This is an info message")
logger.warning("This is a warning message")
logger.error("This is an error message")
logger.critical("This is a critical message")
```

## Log Levels

Loguru uses the following log levels (in order of severity):

| Level    | Numeric Value | When to Use |
|----------|---------------|-------------|
| TRACE    | 5            | Very detailed debugging info |
| DEBUG    | 10           | Detailed debugging info |
| INFO     | 20           | General information |
| SUCCESS  | 25           | Success messages |
| WARNING  | 30           | Warning messages |
| ERROR    | 40           | Error messages |
| CRITICAL | 50           | Critical error messages |

## Environment Variable Configuration

### Setting Log Level

```bash
# Set the minimum log level to display
export LOGURU_LEVEL="DEBUG"    # Show DEBUG and above
export LOGURU_LEVEL="INFO"     # Show INFO and above (default)
export LOGURU_LEVEL="WARNING"  # Show WARNING and above
export LOGURU_LEVEL="ERROR"    # Show ERROR and above
```

### Disabling Logging

```bash
# Disable all logging
export LOGURU_LEVEL="CRITICAL"
# or
export LOGURU_DISABLE=1
```

### Custom Format

```bash
# Custom log format
export LOGURU_FORMAT="{time} | {level} | {message}"
```

## Programmatic Configuration

### Removing Default Handler

```python
from loguru import logger

# Remove the default stderr handler
logger.remove()

# Add your custom handler
logger.add("app.log", level="DEBUG")
```

### Multiple Handlers

```python
from loguru import logger

logger.remove()  # Remove default handler

# Console output (INFO and above)
logger.add(sys.stderr, level="INFO")

# File output (DEBUG and above)
logger.add("debug.log", level="DEBUG", rotation="10 MB")

# Error file (ERROR and above)
logger.add("error.log", level="ERROR", retention="1 week")
```

### Conditional Logging

```python
import os
from loguru import logger

# Configure based on environment
if os.getenv("DEBUG"):
    logger.add("debug.log", level="DEBUG")
else:
    logger.add("app.log", level="INFO")
```

## Best Practices

### Avoid `print()`

In general, you should use `logger` calls instead of `print()` statements. This allows for configurable log levels, structured logging, and directing output to different places (like files or external services) without changing your application code.

❌ **Don't do this:**
```python
print("Something happened")
print(f"Value is {my_variable}")
```

✅ **Do this instead:**
```python
logger.info("Something happened")
logger.debug("Value is {value}", value=my_variable)
```

### String Formatting

❌ **Don't do this:**
```python
logger.info("User " + username + " logged in")
logger.info("Processing {} items".format(count))
```

✅ **Do this instead:**
```python
logger.info("User {username} logged in", username=username)
logger.info("Processing {count} items", count=count)
```

### Structured Logging

```python
logger.info("User action", user_id=123, action="login", ip="192.168.1.1")
```

### Exception Logging

```python
try:
    risky_operation()
except Exception:
    logger.exception("An error occurred")  # Automatically includes traceback
```

## Using the Pre-configured Test Logger

This project has a `test_logger` pytest fixture already configured in `conftest.py`. You don't need to set up loguru yourself - just use the fixture!

### Basic Test Usage

```python
def test_my_function(test_logger):
    """The test_logger fixture is automatically injected into your test."""
    test_logger.info("Starting test")

    result = my_function()
    test_logger.debug("Function returned: {result}", result=result)

    assert result == expected
    test_logger.success("Test passed!")
```

### The Fixture is Auto-Used

The `test_logger` fixture has `autouse=True`, so it's available in ALL tests automatically:

```python
def test_without_explicit_fixture(test_logger):
    """test_logger is automatically available even without requesting it."""
    test_logger.warning("This works automatically")

    # Your test code here
    test_logger.info("Test completed")
```

### Log Levels in Tests

Use appropriate log levels for different types of information:

```python
def test_complex_operation(test_logger):
    test_logger.info("Starting complex operation test")

    # Debug information for troubleshooting
    test_logger.debug("Input parameters: {params}", params=input_data)

    try:
        result = complex_operation(input_data)
        test_logger.success("Operation completed successfully")

        # Log important intermediate results
        test_logger.info("Result length: {length}", length=len(result))

    except Exception as e:
        test_logger.error("Operation failed: {error}", error=str(e))
        raise

    assert len(result) > 0
    test_logger.info("Test assertions passed")
```

### Per-Test Logger Context

Each test gets its own logger context with the test name automatically included:

```python
def test_matmul(test_logger):
    # Logger automatically includes test name in context
    test_logger.info("Testing matmul operation")

    # Your test logic here
```

### Setting Log Level for Tests

You can control test logging verbosity via environment variables:

```bash
# Show all log levels including debug
export LOGURU_LEVEL="DEBUG"
pytest tests/

# Show only info and above (default)
export LOGURU_LEVEL="INFO"
pytest tests/

# Show only warnings and errors
export LOGURU_LEVEL="WARNING"
pytest tests/
```

## Common Environment Setup

Create a `.env` file:
```bash
# Development
LOGURU_LEVEL=DEBUG

# Production
LOGURU_LEVEL=INFO
LOGURU_FORMAT={time:YYYY-MM-DD HH:mm:ss} | {level} | {name}:{function}:{line} - {message}
```

Load in your application:
```python
import os
from loguru import logger

# Configure from environment
log_level = os.getenv("LOGURU_LEVEL", "INFO")
log_format = os.getenv("LOGURU_FORMAT", "<green>{time}</green> | <level>{level}</level> | {message}")

logger.remove()
logger.add(sys.stderr, level=log_level, format=log_format)
```
