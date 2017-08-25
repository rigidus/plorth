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
#include <plorth/token.hpp>

#include "./utils.hpp"

namespace plorth
{
  static bool parse_value(const ref<context>& ctx,
                          std::vector<token>::const_iterator&,
                          const std::vector<token>::const_iterator&,
                          ref<value>&);
  static bool parse_declaration(const ref<context>&,
                                std::vector<token>::const_iterator&,
                                const std::vector<token>::const_iterator&);

  namespace
  {
    /**
     * Compiled quote consists from sequence of tokens parsed from source code.
     * When called, the tokens are being iterated, string, array, object and
     * quote literals are parsed, converted into appropriate values and placed
     * into the stack, while words are being called under the execution context.
     */
    class compiled_quote : public quote
    {
    public:
      explicit compiled_quote(const std::vector<token>& tokens)
        : m_tokens(tokens) {}

      inline enum quote_type quote_type() const
      {
        return quote_type_compiled;
      }

      bool call(const ref<context>& ctx) const
      {
        auto current = std::begin(m_tokens);
        const auto end = std::end(m_tokens);

        while (current != end)
        {
          const class token& token = *current;

          switch (token.type())
          {
            case token::type_string:
            case token::type_lparen:
            case token::type_lbrack:
            case token::type_lbrace:
              {
                ref<value> val;

                if (!parse_value(ctx, current, end, val))
                {
                  return false;
                }
                ctx->push(val);
              }
              break;

            case token::type_colon:
              if (!parse_declaration(ctx, current, end))
              {
                return false;
              }
              break;

            case token::type_word:
              if (!ctx->call(current++->text()))
              {
                return false;
              }
              break;

            case token::type_rparen:
            case token::type_rbrack:
            case token::type_rbrace:
            case token::type_comma:
            case token::type_semicolon:
              ctx->error(
                error::code_syntax,
                U"Unexpected `" + current->to_source() + U"'"
              );

              return false;
          }
        }

        return true;
      }

      unistring to_source() const
      {
        unistring result;
        bool first = true;

        result += '(';
        for (const auto& token : m_tokens)
        {
          if (first)
          {
            first = false;
          } else {
            result += ' ';
          }
          result += token.to_source();
        }
        result += ')';

        return result;
      }

      bool equals(const ref<value>& that) const
      {
        const compiled_quote* q;

        if (!that->is(type_quote) || that.cast<quote>()->is(quote_type_compiled))
        {
          return false;
        }
        q = that.cast<compiled_quote>();
        if (m_tokens.size() != q->m_tokens.size())
        {
          return false;
        }
        for (std::size_t i = 0; i < m_tokens.size(); ++i)
        {
          if (m_tokens[i] != q->m_tokens[i])
          {
            return false;
          }
        }

        return true;
      }

    private:
      const std::vector<token> m_tokens;
    };

    /**
     * Native quotes are wrappers around native C++ functions, allowing the
     * interpreter binary to interoperate with Plorth source code.
     */
    class native_quote : public quote
    {
    public:
      explicit native_quote(callback cb)
        : m_callback(cb) {}

      inline enum quote_type quote_type() const
      {
        return quote_type_native;
      }

      bool call(const ref<context>& ctx) const
      {
        m_callback(ctx);

        return !ctx->error();
      }

      unistring to_source() const
      {
        return U"(\"native quote\")";
      }

      bool equals(const ref<value>& that) const
      {
        // Currently there is no way to compare two std::function instances
        // against each other, even when they are the same type.
        return this == that.get();
      }

    private:
      const callback m_callback;
    };

    /**
     * Curried quote consists from value and quote. When called, the value is
     * placed into the stack and the wrapped quote is called.
     */
    class curried_quote : public quote
    {
    public:
      explicit curried_quote(const ref<value>& argument, const ref<class quote>& quote)
        : m_argument(argument)
        , m_quote(quote) {}

      inline enum quote_type quote_type() const
      {
        return quote_type_curried;
      }

      bool call(const ref<context>& ctx) const
      {
        ctx->push(m_argument);

        return m_quote->call(ctx);
      }

      bool equals(const ref<value>& that) const
      {
        const curried_quote* q;

        if (!that->is(type_quote) || !that.cast<quote>()->is(quote_type_curried))
        {
          return false;
        }
        q = that.cast<curried_quote>();

        return m_argument->equals(q->m_argument) && m_quote->equals(q->m_quote);
      }

      unistring to_source() const
      {
        unistring result;

        result += m_argument->to_source();
        result += ' ';
        result += m_quote->to_source();
        result += U" curry";

        return result;
      }

    private:
      const ref<value> m_argument;
      const ref<quote> m_quote;
    };

    /**
     * Composed quote consists from two quotes that are being called in
     * sequence.
     */
    class composed_quote : public quote
    {
    public:
      explicit composed_quote(const ref<quote>& left, const ref<quote>& right)
        : m_left(left)
        , m_right(right) {}

      inline enum quote_type quote_type() const
      {
        return quote_type_composed;
      }

      bool call(const ref<context>& ctx) const
      {
        return m_left->call(ctx) && m_right->call(ctx);
      }

      bool equals(const ref<value>& that) const
      {
        const composed_quote* q;

        if (!that->is(type_quote) || !that.cast<quote>()->is(quote_type_composed))
        {
          return false;
        }
        q = that.cast<composed_quote>();

        return m_left->equals(q->m_left) && m_right->equals(q->m_right);
      }

      unistring to_source() const
      {
        unistring result;

        result += m_left->to_source();
        result += ' ';
        result += m_right->to_source();
        result += U" compose";

        return result;
      }

    private:
      const ref<quote> m_left;
      const ref<quote> m_right;
    };

    /**
     * Negated quote calls another quote and negates it's boolean result.
     */
    class negated_quote : public quote
    {
    public:
      explicit negated_quote(const ref<class quote>& quote)
        : m_quote(quote) {}

      inline enum quote_type quote_type() const
      {
        return quote_type_negated;
      }

      bool call(const ref<context>& ctx) const
      {
        bool result;

        if (!m_quote->call(ctx) || !ctx->pop_boolean(result))
        {
          return false;
        }
        ctx->push_boolean(!result);

        return true;
      }

      bool equals(const ref<value>& that) const
      {
        const negated_quote* q;

        if (!that->is(type_quote) || !that.cast<quote>()->is(quote_type_negated))
        {
          return false;
        }
        q = that.cast<negated_quote>();

        return m_quote->equals(q->m_quote);
      }

      unistring to_source() const
      {
        return m_quote->to_source() + U" negate";
      }

    private:
      const ref<quote> m_quote;
    };

    /**
     * Constant quote consists from single value. When called, the value is just
     * placed into the stack and nothing else happens.
     */
    class constant_quote : public quote
    {
    public:
      explicit constant_quote(const ref<value>& val)
        : m_value(val) {}

      bool call(const ref<context>& ctx) const
      {
        ctx->push(m_value);

        return true;
      }

      inline enum quote_type quote_type() const
      {
        return quote_type_constant;
      }

      bool equals(const ref<value>& that) const
      {
        const constant_quote* q;

        if (!that->is(type_quote) || !that.cast<quote>()->is(quote_type_negated))
        {
          return false;
        }
        q = that.cast<constant_quote>();

        return m_value->equals(q->m_value);
      }

      unistring to_source() const
      {
        unistring result;

        result += '(';
        if (m_value)
        {
          result += m_value->to_source();
        } else {
          result += U"null";
        }
        result += ')';

        return result;
      }

    private:
      const ref<value> m_value;
    };
  }

  ref<quote> runtime::compiled_quote(const std::vector<token>& tokens)
  {
    return new (*m_memory_manager) class compiled_quote(tokens);
  }

  ref<quote> runtime::native_quote(quote::callback callback)
  {
    return new (*m_memory_manager) class native_quote(callback);
  }

  ref<quote> runtime::curry(const ref<class value>& argument, const ref<class quote>& quote)
  {
    return new (*m_memory_manager) curried_quote(argument, quote);
  }

  ref<quote> runtime::compose(const ref<class quote>& left, const ref<class quote>& right)
  {
    return new (*m_memory_manager) composed_quote(left, right);
  }

  ref<quote> runtime::constant(const ref<class value>& value)
  {
    return new (*m_memory_manager) constant_quote(value);
  }

  unistring quote::to_string() const
  {
    return to_source();
  }

  static ref<quote> parse_quote(const ref<context>& ctx,
                                std::vector<token>::const_iterator& it,
                                const std::vector<token>::const_iterator& end)
  {
    std::vector<token> result;
    int counter = 1;

    while (it != end)
    {
      const class token& token = *it++;

      if (token.is(token::type_lparen))
      {
        ++counter;
      }
      else if (token.is(token::type_rparen) && !--counter)
      {
        break;
      }
      result.push_back(token);
    }
    if (counter > 0)
    {
      ctx->error(error::code_syntax, U"Unterminated quote.");

      return ref<quote>();
    }

    return ctx->runtime()->value<compiled_quote>(result);
  }

  static ref<array> parse_array(const ref<context>& ctx,
                                std::vector<token>::const_iterator& it,
                                const std::vector<token>::const_iterator& end)
  {
    std::vector<ref<value>> elements;
    ref<value> val;

    for (;;)
    {
      if (it >= end)
      {
        ctx->error(error::code_syntax, U"Unterminated array literal.");

        return ref<array>();
      }
      else if (it->is(token::type_rbrack))
      {
        ++it;
        break;
      }
      if (!parse_value(ctx, it, end, val))
      {
        return ref<array>();
      }
      elements.push_back(val);
      if (it >= end)
      {
        ctx->error(error::code_syntax, U"Unterminated array literal.");

        return ref<array>();
      }
      else if (it->is(token::type_comma))
      {
        ++it;
      }
      else if (!it->is(token::type_rbrack))
      {
        ctx->error(
          error::code_syntax,
          U"Unexpected `" + it->to_source() + U"'; Missing `]'"
        );

        return ref<array>();
      }
    }

    return ctx->runtime()->array(elements.data(), elements.size());
  }

  static ref<object> parse_object(const ref<context>& ctx,
                                  std::vector<token>::const_iterator& it,
                                  const std::vector<token>::const_iterator& end)
  {
    object::container_type properties;
    unistring id;
    ref<value> val;

    for (;;)
    {
      if (it >= end)
      {
        ctx->error(error::code_syntax, U"Unterminated object literal.");

        return ref<object>();
      }
      else if (it->is(token::type_rbrace))
      {
        ++it;
        break;
      }
      else if (!it->is(token::type_string))
      {
        ctx->error(error::code_syntax, U"Missing key for object literal.");

        return ref<object>();
      }
      id = it++->text();
      if (it >= end || !it++->is(token::type_colon))
      {
        ctx->error(error::code_syntax, U"Missing `:' after key of an object.");

        return ref<object>();
      }
      else if (!parse_value(ctx, it, end, val))
      {
        return ref<object>();
      }
      properties[id] = val;
      if (it >= end)
      {
        ctx->error(error::code_syntax, U"Unterminated object literal.");

        return ref<object>();
      }
      else if (it->is(token::type_comma))
      {
        ++it;
      }
      else if (!it->is(token::type_rbrace))
      {
        ctx->error(
          error::code_syntax,
          U"Unexpected `" + it->to_source() + U"'; Missing `]'"
        );

        return ref<object>();
      }
    }

    return ctx->runtime()->value<object>(properties);
  }

  static bool parse_value(const ref<context>& ctx,
                          std::vector<token>::const_iterator& it,
                          const std::vector<token>::const_iterator& end,
                          ref<value>& slot)
  {
    const class token& token = *it++;

    switch (token.type())
    {
      case token::type_string:
        slot = ctx->runtime()->string(token.text());
        break;

      case token::type_lparen:
        if (!(slot = parse_quote(ctx, it, end)))
        {
          return false;
        }
        break;

      case token::type_lbrack:
        if (!(slot = parse_array(ctx, it, end)))
        {
          return false;
        }
        break;

      case token::type_lbrace:
        if (!(slot = parse_object(ctx, it, end)))
        {
          return false;
        }
        break;

      case token::type_word:
        {
          const unistring& text = token.text();

          if (!text.compare(U"null"))
          {
            slot.release();
            break;
          }
          else if (!text.compare(U"true"))
          {
            slot = ctx->runtime()->true_value();
            break;
          }
          else if (!text.compare(U"false"))
          {
            slot = ctx->runtime()->false_value();
            break;
          }
          else if (!text.compare(U"drop"))
          {
            if (!ctx->pop(slot))
            {
              return false;
            }
            break;
          }
          else if (is_number(text))
          {
            slot = ctx->runtime()->number(text);
            break;
          }
        }

      default:
        ctx->error(
          error::code_syntax,
          U"Unexpected `" + token.to_source() + U"', missing value."
        );

        return false;
    }

    return true;
  }

  static bool parse_declaration(const ref<context>& ctx,
                                std::vector<token>::const_iterator& current,
                                const std::vector<token>::const_iterator& end)
  {
    unistring name;
    std::vector<token> tokens;
    int counter = 1;

    if (++current >= end || !current->is(token::type_word))
    {
      ctx->error(error::code_syntax, U"Missing name after word declaration.");

      return false;
    }
    name = current++->text();
    while (current != end)
    {
      const class token& token = *current++;

      if (token.is(token::type_colon))
      {
        ++counter;
      }
      else if (token.is(token::type_semicolon) && !--counter)
      {
        break;
      }
      tokens.push_back(token);
    }
    if (counter > 0)
    {
      ctx->error(error::code_syntax, U"Unterminated declaration.");

      return false;
    }
    ctx->declare(name, ctx->runtime()->value<compiled_quote>(tokens));

    return true;
  }

  /**
   * Word: call
   * Prototype: quote
   *
   * Takes:
   * - quote
   *
   * Executes the quote taken from the top of the stack.
   */
  static void w_call(const ref<context>& ctx)
  {
    ref<quote> q;

    if (ctx->pop_quote(q))
    {
      q->call(ctx);
    }
  }

  /**
   * Word: compose
   * Prototype: quote
   *
   * Takes:
   * - quote
   * - quote
   *
   * Gives:
   * - quote
   *
   * Constructs a new quote which will call the two given quotes in sequence.
   */
  static void w_compose(const ref<context>& ctx)
  {
    ref<quote> left;
    ref<quote> right;

    if (ctx->pop_quote(right) && ctx->pop_quote(left))
    {
      ctx->push(ctx->runtime()->compose(left, right));
    }
  }

  /**
   * Word: curry
   * Prototype: quote
   *
   * Takes:
   * - any
   * - quote
   *
   * Gives:
   * - quote
   *
   * Constructs a curried quote where given value will be pushed onto the stack
   * before calling the original quote.
   */
  static void w_curry(const ref<context>& ctx)
  {
    ref<value> argument;
    ref<quote> q;

    if (ctx->pop_quote(q) && ctx->pop(argument))
    {
      ctx->push(ctx->runtime()->curry(argument, q));
    }
  }

  /**
   * Word: negate
   * Prototype: quote
   *
   * Takes:
   * - quote
   *
   * Gives:
   * - quote
   *
   * Constructs a negated version of given quote which negates the boolean
   * result returned by the original quote.
   */
  static void w_negate(const ref<context>& ctx)
  {
    ref<quote> quo;

    if (ctx->pop_quote(quo))
    {
      ctx->push(ctx->runtime()->value<negated_quote>(quo));
    }
  }

  /**
   * Word: dip
   * Prototype: quote
   *
   * Takes:
   * - any
   * - quote
   *
   * Gives:
   * - any
   *
   * Temporarily hides given value from the stack and calls given quote. Once
   * the quote has returned from it's execution, hidden value will be placed
   * back on the stack.
   */
  static void w_dip(const ref<context>& ctx)
  {
    ref<value> val;
    ref<quote> quo;

    if (!ctx->pop_quote(quo) || !ctx->pop(val))
    {
      return;
    }

    quo->call(ctx);
    ctx->push(val);
  }

  /**
   * Word: 2dip
   * Prototype: quote
   *
   * Takes:
   * - any
   * - any
   * - quote
   *
   * Gives:
   * - any
   * - any
   *
   *
   * Temporarily hides two given values from the stack and calls given quote.
   * Once the quote has returned from it's execution, hidden values will be
   * placed back on the stack.
   */
  static void w_2dip(const ref<context>& ctx)
  {
    ref<value> val1;
    ref<value> val2;
    ref<quote> quo;

    if (!ctx->pop_quote(quo) || !ctx->pop(val2) || !ctx->pop(val1))
    {
      return;
    }

    quo->call(ctx);
    ctx->push(val1);
    ctx->push(val2);
  }

  namespace api
  {
    runtime::prototype_definition quote_prototype()
    {
      return
      {
        { U"call", w_call },
        { U"compose", w_compose },
        { U"curry", w_curry },
        { U"negate", w_negate },
        { U"dip", w_dip },
        { U"2dip", w_2dip }
      };
    }
  }
}
