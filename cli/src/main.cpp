/*
 * Copyright (c) 2017-2018, Rauli Laine
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <plorth/plorth.hpp>
#include <plorth/cli/config.hpp>

#include <cstring>
#include <fstream>
#include <stack>
#if PLORTH_ENABLE_MODULES
# include <unordered_set>
#endif

#if defined(HAVE_UNISTD_H)
# include <unistd.h>
#endif
#if defined(HAVE_SYSEXITS_H)
# include <sysexits.h>
#endif

#include <linenoise.h>

#if !defined(EX_USAGE)
# define EX_USAGE 64
#endif

using namespace plorth;

static const char* script_filename = nullptr;
static bool flag_test_syntax = false;
static bool flag_fork = false;
static std::string inline_script;
#if PLORTH_ENABLE_MODULES
static std::unordered_set<unistring> imported_modules;
#endif

static void scan_arguments(const std::shared_ptr<runtime>&, int, char**);
#if PLORTH_ENABLE_MODULES
static void scan_module_path(const std::shared_ptr<runtime>&);
#endif
static inline bool is_console_interactive();
static void compile_and_run(const std::shared_ptr<context>&,
                            const std::string&,
                            const unistring&);
static void console_loop(const std::shared_ptr<context>&);
static void handle_error(const std::shared_ptr<context>&);

void initialize_repl_api(const std::shared_ptr<runtime>&);

int main(int argc, char** argv)
{
  memory::manager memory_manager;
  auto runtime = runtime::make(memory_manager);
  auto context = context::make(runtime);

#if PLORTH_ENABLE_MODULES
  scan_module_path(runtime);
#endif

  scan_arguments(runtime, argc, argv);

#if PLORTH_ENABLE_MODULES
  for (const auto& module_path : imported_modules)
  {
    if (!context->import(module_path))
    {
      handle_error(context);
    }
  }
#endif

  if (script_filename)
  {
    const unistring decoded_script_filename = utf8_decode(script_filename);
    std::ifstream is(script_filename, std::ios_base::in);

    if (is.good())
    {
      const std::string source = std::string(
        std::istreambuf_iterator<char>(is),
        std::istreambuf_iterator<char>()
      );

      is.close();
      context->clear();
#if PLORTH_ENABLE_MODULES
      context->filename(decoded_script_filename);
#endif
      compile_and_run(context, source, decoded_script_filename);
    } else {
      std::cerr << argv[0]
                << ": Unable to open file `"
                << script_filename
                << "' for reading."
                << std::endl;
      std::exit(EXIT_FAILURE);
    }
  }
  else if (!inline_script.empty())
  {
    compile_and_run(context, inline_script, U"-e");
  }
  else if (is_console_interactive())
  {
    console_loop(context);
  } else {
    compile_and_run(
      context,
      std::string(
        std::istreambuf_iterator<char>(std::cin),
        std::istreambuf_iterator<char>()
      ),
      U"<stdin>"
    );
  }

  return EXIT_SUCCESS;
}

static void print_usage(std::ostream& out, const char* executable)
{
  out << std::endl
      << "Usage: "
      << executable
      << " [switches] [--] [programfile] [arguments]"
      << std::endl;
  out << "  -c           Check syntax only." << std::endl;
#if HAVE_FORK
  out << "  -f           Fork to background before executing script."
      << std::endl;
#endif
  out << "  -e <program> One line of program. (Several -e's allowed, omit "
      << "programfile.)"
      << std::endl;
#if PLORTH_ENABLE_MODULES
  out << "  -r <path>    Import module before executing script." << std::endl;
#endif
  out << "  --version    Print the version." << std::endl;
  out << "  --help       Display this message." << std::endl;
  out << std::endl;
}

static void scan_arguments(const std::shared_ptr<class runtime>& runtime,
                           int argc,
                           char** argv)
{
  int offset = 1;

  while (offset < argc)
  {
    const char* arg = argv[offset++];

    if (!*arg)
    {
      continue;
    }
    else if (*arg != '-')
    {
      if (inline_script.empty())
      {
        script_filename = arg;
      }
      break;
    }
    else if (!arg[1])
    {
      break;
    }
    else if (arg[1] == '-')
    {
      if (!std::strcmp(arg, "--help"))
      {
        print_usage(std::cout, argv[0]);
        std::exit(EXIT_SUCCESS);
      }
      else if (!std::strcmp(arg, "--version"))
      {
        std::cerr << "Plorth " << utf8_encode(PLORTH_VERSION) << std::endl;
        std::exit(EXIT_SUCCESS);
      }
      else if (!std::strcmp(arg, "--"))
      {
        if (offset < argc)
        {
          script_filename = argv[offset++];
        }
        break;
      } else {
        std::cerr << "Unrecognized switch: " << arg << std::endl;
        print_usage(std::cerr, argv[0]);
        std::exit(EX_USAGE);
      }
    }
    for (int i = 1; arg[i]; ++i)
    {
      switch (arg[i])
      {
      case 'c':
        flag_test_syntax = true;
        break;

      case 'e':
        if (offset < argc)
        {
          inline_script.append(argv[offset++]);
          inline_script.append('\n', 1);
        } else {
          std::cerr << "Argument expected for the -e option." << std::endl;
          print_usage(std::cerr, argv[0]);
          std::exit(EX_USAGE);
        }
        break;

      case 'f':
        flag_fork = true;
        break;

#if PLORTH_ENABLE_MODULES
      case 'r':
        if (offset < argc)
        {
          unistring module_path;

          if (!utf8_decode_test(argv[offset++], module_path))
          {
            std::cerr << "Unable to decode given module path." << std::endl;
            std::exit(EXIT_FAILURE);
          }
          imported_modules.insert(module_path);
        } else {
          std::cerr << "Argument expected for the -r option." << std::endl;
          print_usage(std::cerr, argv[0]);
          std::exit(EX_USAGE);
        }
        break;
#else
      case 'r':
        std::cerr << "Modules have been disabled." << std::endl;
        std::exit(EXIT_FAILURE);
        break;
#endif

      case 'h':
        print_usage(std::cout, argv[0]);
        std::exit(EXIT_SUCCESS);
        break;

      default:
        std::cerr << "Unrecognized switch: `" << arg[i] << "'" << std::endl;
        print_usage(std::cerr, argv[0]);
        std::exit(EX_USAGE);
      }
    }
  }

  while (offset < argc)
  {
    runtime->arguments().push_back(utf8_decode(argv[offset++]));
  }
}

#if PLORTH_ENABLE_MODULES
static void scan_module_path(const std::shared_ptr<class runtime>& runtime)
{
#if defined(_WIN32)
  static const unichar path_separator = ';';
#else
  static const unichar path_separator = ':';
#endif
  auto& module_paths = runtime->module_paths();
  const char* begin = std::getenv("PLORTHPATH");
  const char* end = begin;

  if (end)
  {
    for (; *end; ++end)
    {
      if (*end != path_separator)
      {
        continue;
      }

      if (end - begin > 0)
      {
        module_paths.push_back(utf8_decode(std::string(begin, end - begin)));
      }
      begin = end + 1;
    }

    if (end - begin > 0)
    {
      module_paths.push_back(utf8_decode(std::string(begin, end - begin)));
    }
  }

#if defined(PLORTH_RUNTIME_LIBRARY_PATH)
  if (module_paths.empty())
  {
    module_paths.push_back(PLORTH_RUNTIME_LIBRARY_PATH);
  }
#endif
}
#endif

static inline bool is_console_interactive()
{
#if defined(HAVE_ISATTY)
  return isatty(fileno(stdin));
#else
  return false;
#endif
}

static void handle_error(const std::shared_ptr<context>& ctx)
{
  const std::shared_ptr<error>& err = ctx->error();

  if (err)
  {
    const auto position = err->position();

    std::cerr << "Error: ";
    if (position && (!position->filename.empty() || position->line))
    {
      std::cerr << *position << ':';
    }
    std::cerr << err->code() << " - " << err->message();
  } else {
    std::cerr << "Unknown error.";
  }
  std::cerr << std::endl;
  std::exit(EXIT_FAILURE);
}

static void compile_and_run(const std::shared_ptr<context>& ctx,
                            const std::string& input,
                            const unistring& filename)
{
  unistring source;
  std::shared_ptr<quote> script;

  if (!utf8_decode_test(input, source))
  {
    std::cerr << "Import error: Unable to decode source code as UTF-8." << std::endl;
    std::exit(EXIT_FAILURE);
  }

  if (!(script = ctx->compile(source, filename)))
  {
    handle_error(ctx);
    return;
  }

  if (flag_test_syntax)
  {
    std::cerr << "Syntax OK." << std::endl;
    std::exit(EXIT_SUCCESS);
    return;
  }

  if (flag_fork)
  {
#if HAVE_FORK
    if (fork())
    {
      std::exit(EXIT_SUCCESS);
    }
#else
    std::cerr << "Forking to background is not supported on this platform." << std::endl;
#endif
  }

  if (!script->call(ctx))
  {
    handle_error(ctx);
  }
}

static void count_open_braces(const char* input, std::stack<char>& open_braces)
{
  const auto length = std::strlen(input);

  for (std::size_t i = 0; i < length; ++i)
  {
    switch (input[i])
    {
      case '#':
        return;

      case '(':
        open_braces.push(')');
        break;

      case '[':
        open_braces.push(']');
        break;

      case '{':
        open_braces.push('}');
        break;

      case ')':
      case ']':
      case '}':
        if (!open_braces.empty() && open_braces.top() == input[i])
        {
          open_braces.pop();
        }
        break;

      case '"':
        while (i < length)
        {
          if (input[i] == '"')
          {
            break;
          }
          else if (input[i] == '\\' && i + 1 < length)
          {
            i += 2;
          } else {
            ++i;
          }
        }
    }
  }
}

static void console_loop(const std::shared_ptr<class context>& context)
{
  int line_counter = 0;
  unistring source;
  std::stack<char> open_braces;
  char prompt[BUFSIZ];

  initialize_repl_api(context->runtime());

  for (;;)
  {
    char* line;

    // First construct the prompt which is shown to the user. It contains text
    // "plorth", current line number, size of the execution context and
    // visual indication on whether the source code still contains open braces
    // or not.
    std::snprintf(
      prompt,
      BUFSIZ,
      "plorth:%d:%ld%c ",
      ++line_counter,
      context->size(),
      open_braces.empty() ? '>' : '*'
    );

    // Read line from the user.
    if (!(line = linenoise(prompt)))
    {
      break;
    }

    // Skip empty lines.
    if (!*line)
    {
      linenoiseFree(line);
      continue;
    }

    // Add the line into history.
    linenoiseHistoryAdd(line);

    // And attempt to decode the input as UTF-8.
    if (!utf8_decode_test(line, source))
    {
      std::cout << "Unable to decode given input as UTF-8." << std::endl;
      linenoiseFree(line);
      continue;
    }

    // Insert new line into the source code so that the line counter
    source.append(1, '\n');

    // See whether the line contains special characters such as open braces and
    // so on.
    count_open_braces(line, open_braces);

    // We no longer need the original line, so free it.
    linenoiseFree(line);

    // Do not attempt to compile the source code when it still has unclosed
    // braces.
    if (!open_braces.empty())
    {
      continue;
    }

    // Attempt to compile the source code into a quote and execute it unless
    // syntax errors were encountered.
    if (auto script = context->compile(source, U"<repl>", line_counter))
    {
      script->call(context);
    }

    // Clear the source code buffer so that we can use it again.
    source.clear();

    // If the execution context has any error present, display it. Also reset
    // the error status so that the execution context can be reused.
    if (const auto& error = context->error())
    {
      if (auto position = error->position())
      {
        std::cout << *position << ':';
      }
      std::cout << error << std::endl;
      context->clear_error();
    }
  }
}
