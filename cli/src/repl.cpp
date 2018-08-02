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
#include <cstring>

#include <plorth/context.hpp>
#include <linenoise.h>
#include "./utils.hpp"

namespace plorth
{
  namespace cli
  {
    void initialize_repl_api(const std::shared_ptr<runtime>&);

    void repl_loop(const std::shared_ptr<context>& ctx)
    {
      int line_counter = 0;
      unistring source;
      std::stack<char32_t> open_braces;
      char prompt[BUFSIZ];

      initialize_repl_api(ctx->runtime());

      for (;;)
      {
        char* line;

        // First construct the prompt which is shown to the user. It contains
        // text "plorth", current line number, size of the execution context
        // and visual indication on whether the source code still contains open
        // braces or not.
        std::snprintf(
          prompt,
          BUFSIZ,
          "plorth:%d:%ld%c ",
          ++line_counter,
          ctx->size(),
          open_braces.empty() ? '>' : '*'
        );

        // Read line from the user.
        if (!(line = ::linenoise(prompt)))
        {
          break;
        }

        // Skip empty lines.
        if (!*line)
        {
          ::linenoiseFree(line);
          continue;
        }

        // Add the line into history.
        ::linenoiseHistoryAdd(line);

        // And attempt to decode the input as UTF-8.
        if (!utf8_decode_test(line, source))
        {
          std::cout << "Unable to decode given input as UTF-8." << std::endl;
          ::linenoiseFree(line);
          continue;
        }

        // Insert new line into the source code so that the line counter
        source.append(1, '\n');

        // See whether the line contains special characters such as open braces
        // and so on.
        utils::count_open_braces(line, std::strlen(line), open_braces);

        // We no longer need the original line, so free it.
        ::linenoiseFree(line);

        // Do not attempt to compile the source code when it still has unclosed
        // braces.
        if (!open_braces.empty())
        {
          continue;
        }

        // Attempt to compile the source code into a quote and execute it
        // unless syntax errors were encountered.
        if (auto script = ctx->compile(source, U"<repl>", line_counter))
        {
          script->call(ctx);
        }

        // Clear the source code buffer so that we can use it again.
        source.clear();

        // If the execution context has any error present, display it. Also
        // reset the error status so that the execution context can be reused.
        if (const auto& error = ctx->error())
        {
          if (auto position = error->position())
          {
            std::cout << *position << ':';
          }
          std::cout << error << std::endl;
          ctx->clear_error();
        }
      }
    }
  }
}