#include <sstream>

#include "state.hpp"

namespace plorth
{
  Error::Error(ErrorCode code, const std::string& message)
    : m_code(code)
    , m_message(message) {}

  Ref<Object> Error::GetPrototype(const Ref<Runtime>& runtime) const
  {
    Ref<Value> value;

    if (runtime->FindWord("error", value) && value->GetType() == TYPE_OBJECT)
    {
      return value.As<Object>();
    } else {
      return Ref<Object>();
    }
  }

  bool Error::Equals(const Ref<Value>& that) const
  {
    Ref<Error> other;

    if (that->GetType() != TYPE_ERROR)
    {
      return false;
    }
    other = that.As<Error>();

    return m_code == other->m_code && m_message == other->m_message;
  }

  std::string Error::ToString() const
  {
    std::stringstream ss;

    ss << m_code;
    if (!m_message.empty())
    {
      ss << ": " << m_message;
    }

    return ss.str();
  }

  std::string Error::ToSource() const
  {
    return "<" + ToString() + ">";
  }

  std::ostream& operator<<(std::ostream& os, Error::ErrorCode code)
  {
    const char* description = "unknown error";

    switch (code)
    {
      case Error::ERROR_CODE_SYNTAX:
        description = "syntax error";
        break;

      case Error::ERROR_CODE_REFERENCE:
        description = "reference error";
        break;

      case Error::ERROR_CODE_TYPE:
        description = "type error";
        break;

      case Error::ERROR_CODE_RANGE:
        description = "range error";
        break;

      case Error::ERROR_CODE_UNKNOWN:
        description = "unknown error";
        break;
    }
    os << description;

    return os;
  }

  /**
   * error? ( any -- any bool )
   *
   * Returns true if given value is error.
   */
  static void w_is_error(const Ref<State>& state)
  {
    Ref<Value> value;

    if (state->Peek(value))
    {
      state->PushBool(value->GetType() == Value::TYPE_ERROR);
    }
  }

  /**
   * try ( quote quote -- )
   *
   * Executes first quote and if it throws an error, calls second quote with
   * the error on top of the stack.
   */
  static void w_try(const Ref<State>& state)
  {
    Ref<Quote> try_block;
    Ref<Quote> catch_block;

    if (!state->PopQuote(catch_block) || !state->PopQuote(try_block))
    {
      return;
    }
    if (!try_block->Call(state))
    {
      state->Push(state->GetError());
      state->ClearError();
      catch_block->Call(state);
    }
  }

  /**
   * code ( error -- error num )
   *
   * Returns error code extracted from the error in numeric form.
   */
  static void w_code(const Ref<State>& state)
  {
    Ref<Error> error;

    if (state->PeekError(error))
    {
      state->PushNumber(static_cast<std::int64_t>(error->GetCode()));
    }
  }

  /**
   * message ( error -- error str|null )
   *
   * Returns error message extracted from the error, or null if the error does
   * not have any error message.
   */
  static void w_message(const Ref<State>& state)
  {
    Ref<Error> error;

    if (state->PeekError(error))
    {
      const std::string& message = error->GetMessage();

      if (message.empty())
      {
        state->PushNull();
      } else {
        state->PushString(message);
      }
    }
  }

  /**
   * throw ( error -- )
   *
   * Sets given error as current error of the execution state.
   */
  static void w_throw(const Ref<State>& state)
  {
    Ref<Error> error;

    if (state->PopError(error))
    {
      state->SetError(error);
    }
  }

  void api_init_error(Runtime* runtime)
  {
    runtime->AddWord("error?", w_is_error);
    runtime->AddWord("try", w_try);

    runtime->AddNamespace("error", {
      { "code", w_code },
      { "message", w_message },
      { "throw", w_throw },
    });
  }
}