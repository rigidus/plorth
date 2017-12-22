/*
 * Copyright (c) 2017, Rauli Laine
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
#include <plorth/context.hpp>
#include <plorth/value-word.hpp>

namespace plorth
{
  word::word(const ref<class symbol>& symbol, const ref<class quote>& quote)
    : m_symbol(symbol)
    , m_quote(quote) {}

  bool word::equals(const ref<value>& that) const
  {
    word* w;

    if (!that || !that->is(type_word))
    {
      return false;
    }
    w = that.cast<word>();

    return m_symbol->equals(w->m_symbol) && m_quote->equals(w->m_quote);
  }

  bool word::exec(const ref<context>& ctx)
  {
    auto& dictionary = ctx->dictionary();

    dictionary[m_symbol->id()] = m_quote;

    return true;
  }

  bool word::eval(const ref<context>& ctx, ref<value>&)
  {
    ctx->error(
      error::code_syntax,
      U"Unexpected word declaration; Missing value."
    );

    return false;
  }

  unistring word::to_string() const
  {
    return to_source();
  }

  unistring word::to_source() const
  {
    return U": " + m_symbol->id() + U" " + m_quote->to_string() + U" ;";
  }

  /**
   * Word: symbol
   * Prototype: word
   *
   * Takes:
   * - word
   *
   * Gives:
   * - word
   * - symbol
   *
   * Extracts symbol from the word and places it onto top of the stack.
   */
  static void w_symbol(const ref<context>& ctx)
  {
    ref<word> wrd;

    if (ctx->pop_word(wrd))
    {
      ctx->push(wrd);
      ctx->push(wrd->symbol());
    }
  }

  /**
   * Word: quote
   * Prototype: word
   *
   * Takes:
   * - word
   *
   * Gives:
   * - word
   * - quote
   *
   * Extracts quote which acts as the body of the word and places it onto top
   * of the stack.
   */
  static void w_quote(const ref<context>& ctx)
  {
    ref<word> wrd;

    if (ctx->pop_word(wrd))
    {
      ctx->push(wrd);
      ctx->push(wrd->quote());
    }
  }

  /**
   * Word: call
   * Prototype: word
   *
   * Takes:
   * - word
   *
   * Executes body of the given word.
   */
  static void w_call(const ref<context>& ctx)
  {
    ref<word> wrd;

    if (ctx->pop_word(wrd))
    {
      wrd->quote()->call(ctx);
    }
  }

  /**
   * Word: define
   * Prototype: word
   *
   * Takes:
   * - word
   *
   * Inserts given word into current local dictionary.
   */
  void w_define(const ref<context>& ctx)
  {
    ref<word> wrd;

    if (ctx->pop_word(wrd))
    {
      wrd->exec(ctx);
    }
  }

  namespace api
  {
    runtime::prototype_definition word_prototype()
    {
      return
      {
        { U"symbol", w_symbol },
        { U"quote", w_quote },

        { U"call", w_call },
        { U"define", w_define }
      };
    }
  }
}