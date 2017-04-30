#include <plorth/plorth-context.hpp>

namespace plorth
{
  /**
   * len ( ary -- ary num )
   *
   * Returns number of elements in the array.
   */
  static void w_len(const Ref<Context>& context)
  {
    Ref<Array> array;

    if (context->PeekArray(array))
    {
      context->PushNumber(static_cast<std::int64_t>(array->GetElements().size()));
    }
  }

  /**
   * empty? ( ary -- ary bool )
   *
   * Returns true if the array is empty.
   */
  static void w_is_empty(const Ref<Context>& context)
  {
    Ref<Array> array;

    if (context->PeekArray(array))
    {
      context->PushBool(array->GetElements().empty());
    }
  }

  /**
   * every? ( quote ary -- bool )
   *
   * Tests whether all elements in the array passes the test implemented by the
   * provided quote.
   */
  static void w_every(const Ref<Context>& context)
  {
    Ref<Array> array;
    Ref<Quote> quote;

    if (!context->PopArray(array) || !context->PopQuote(quote))
    {
      return;
    }
    for (const auto& element : array->GetElements())
    {
      bool result;

      context->Push(element);
      if (!quote->Call(context) || !context->PopBool(result))
      {
        return;
      }
      else if (!result)
      {
        context->PushBool(false);
        return;
      }
    }
    context->PushBool(true);
  }

  /**
   * some? ( quote ary -- bool )
   *
   * Tests whether any element in the array passes the test implemented by the
   * provided quote.
   */
  static void w_some(const Ref<Context>& context)
  {
    Ref<Array> array;
    Ref<Quote> quote;

    if (!context->PopArray(array) || !context->PopQuote(quote))
    {
      return;
    }
    for (const auto& element : array->GetElements())
    {
      bool result;

      context->Push(element);
      if (!quote->Call(context) || !context->PopBool(result))
      {
        return;
      }
      else if (result)
      {
        context->PushBool(true);
        return;
      }
    }
    context->PushBool(false);
  }

  /**
   * index-of ( any ary -- num|null )
   *
   * Attempts to find given value from the array and returns index of it, if it
   * exists in the array, otherwise null.
   */
  static void w_index_of(const Ref<Context>& context)
  {
    Ref<Array> array;
    Ref<Value> value;

    if (!context->PopArray(array) || !context->Pop(value))
    {
      return;
    }
    for (std::size_t i = 0; i < array->GetElements().size(); ++i)
    {
      const Ref<Value>& element = array->GetElements()[i];

      if (element->Equals(value))
      {
        context->PushNumber(static_cast<std::int64_t>(i));
        return;
      }
    }
    context->PushNull();
  }

  /**
   * join ( str ary -- str )
   *
   * Concatenates all elements from the array into single string, delimited by
   * the given separator string.
   */
  static void w_join(const Ref<Context>& context)
  {
    Ref<Array> array;
    Ref<String> separator;

    if (context->PopArray(array) && context->PopString(separator))
    {
      std::string result;
      bool first = true;

      for (auto& element : array->GetElements())
      {
        if (first)
        {
          first = false;
        } else {
          result += separator->GetValue();
        }
        result += element->ToString();
      }
      context->PushString(result);
    }
  }

  /**
   * for-each ( quote ary -- )
   *
   * Runs quote once for every element in the array.
   */
  static void w_for_each(const Ref<Context>& context)
  {
    Ref<Array> array;
    Ref<Quote> quote;

    if (context->PopArray(array) && context->PopQuote(quote))
    {
      for (const auto& element : array->GetElements())
      {
        context->Push(element);
        if (!quote->Call(context))
        {
          return;
        }
      }
    }
  }

  /**
   * filter ( quote ary -- ary )
   *
   * Applies quote once for each element in the array and constructs new array
   * from ones which passed the test.
   */
  static void w_filter(const Ref<Context>& context)
  {
    Ref<Array> array;
    Ref<Quote> quote;
    std::vector<Ref<Value>> result;

    if (!context->PopArray(array) || !context->PopQuote(quote))
    {
      return;
    }
    for (const auto& element : array->GetElements())
    {
      bool quote_result;

      context->Push(element);
      if (!quote->Call(context) || !context->PopBool(quote_result))
      {
        return;
      }
      else if (quote_result)
      {
        result.push_back(element);
      }
    }
    context->PushArray(result);
  }

  /**
   * map ( quote ary -- ary )
   *
   * Applies quote once for each element in the array and constructs new array
   * from values returned by the quote.
   */
  static void w_map(const Ref<Context>& context)
  {
    Ref<Array> array;
    Ref<Quote> quote;
    std::vector<Ref<Value>> result;

    if (!context->PopArray(array) || !context->PopQuote(quote))
    {
      return;
    }
    for (const auto& element : array->GetElements())
    {
      Ref<Value> quote_result;

      context->Push(element);
      if (!quote->Call(context) || !context->Pop(quote_result))
      {
        return;
      }
      result.push_back(quote_result);
    }
    context->PushArray(result);
  }

  /**
   * reduce ( quote ary -- any )
   *
   * Applies given quote against an acculumator and each element in array to
   * reduce it into single value.
   */
  static void w_reduce(const Ref<Context>& context)
  {
    Ref<Array> array;
    Ref<Quote> quote;
    Ref<Value> result;
    std::size_t size;

    if (!context->PopArray(array) || !context->PopQuote(quote))
    {
      return;
    }
    size = array->GetElements().size();
    if (size == 0)
    {
      context->SetError(Error::ERROR_CODE_RANGE, "Cannot reduce empty array.");
      return;
    }
    result = array->GetElements()[0];
    for (std::size_t i = 1; i < size; ++i)
    {
      context->Push(result);
      context->Push(array->GetElements()[i]);
      if (!quote->Call(context) || !context->Pop(result))
      {
        return;
      }
    }
    context->Push(result);
  }

  /**
   * find ( quote ary -- num|null )
   *
   * Returns index of the element in the array which passes test implemented by
   * the given quote, or null if no such element exists in the array.
   */
  static void w_find(const Ref<Context>& context)
  {
    Ref<Array> array;
    Ref<Quote> quote;

    if (!context->PopArray(array) || !context->PopQuote(quote))
    {
      return;
    }
    for (std::size_t i = 0; i < array->GetElements().size(); ++i)
    {
      const Ref<Value>& element = array->GetElements()[i];
      bool result;

      context->Push(element);
      if (!quote->Call(context) || !context->PopBool(result))
      {
        return;
      }
      else if (result)
      {
        context->PushNumber(static_cast<std::int64_t>(i));
        return;
      }
    }
    context->PushNull();
  }

  /**
   * reverse ( ary -- ary )
   *
   * Returns copy of the array in reversed order.
   */
  static void w_reverse(const Ref<Context>& context)
  {
    Ref<Array> array;

    if (context->PopArray(array))
    {
      const std::vector<Ref<Value>>& elements = array->GetElements();

      context->PushArray(
        std::vector<Ref<Value>>(
          elements.rbegin(),
          elements.rend()
        )
      );
    }
  }

  /**
   * extract ( ary -- any... )
   *
   * Extracts all values from the array and pushes them to the stack.
   */
  static void w_extract(const Ref<Context>& context)
  {
    Ref<Array> array;

    if (context->PopArray(array))
    {
      const std::vector<Ref<Value>>& elements = array->GetElements();

      for (std::size_t i = 0; i < elements.size(); ++i)
      {
        context->Push(elements[i]);
      }
    }
  }

  /**
   * @ ( num ary -- any )
   *
   * Retrieves value from array at given index. Negative indexes count
   * backwards.
   */
  static void w_get(const Ref<Context>& context)
  {
    Ref<Array> array;
    Ref<Number> index;

    if (context->PopArray(array) && context->PopNumber(index))
    {
      const std::vector<Ref<Value>>& elements = array->GetElements();
      std::int64_t i = index->AsInt();

      if (i < 0)
      {
        i += elements.size();
      }
      if (i < 0 || static_cast<std::size_t>(i) >= elements.size())
      {
        context->SetError(Error::ERROR_CODE_RANGE, "Array index out of bounds.");
        return;
      }
      context->Push(elements[i]);
    }
  }

  /**
   * ! ( any num ary -- any )
   *
   * Sets value in the array at given index. Negative indexes count backwrds.
   * If index is larger than number of elements in the array, value will be
   * appended as the last element of the array.
   */
  static void w_set(const Ref<Context>& context)
  {
    Ref<Array> array;
    Ref<Number> index;
    Ref<Value> value;

    if (context->PopArray(array) && context->PopNumber(index) && context->Pop(value))
    {
      std::vector<Ref<Value>> elements = array->GetElements();
      std::int64_t i = index->AsInt();

      if (i < 0)
      {
        i += elements.size();
      }
      if (i < 0 || static_cast<std::size_t>(i) >= elements.size())
      {
        elements.push_back(value);
      } else {
        elements[i] = value;
      }
      context->PushArray(elements);
    }
  }

  /**
   * + ( ary ary -- ary )
   *
   * Combines contents of two arrays together.
   */
  static void w_plus(const Ref<Context>& context)
  {
    Ref<Array> array_a;
    Ref<Array> array_b;

    if (context->PopArray(array_a) && context->PopArray(array_b))
    {
      const std::vector<Ref<Value>>& a = array_a->GetElements();
      const std::vector<Ref<Value>>& b = array_b->GetElements();
      std::vector<Ref<Value>> result;

      result.reserve(a.size() + b.size());
      result.insert(end(result), begin(b), end(b));
      result.insert(end(result), begin(a), end(a));
      context->PushArray(result);
    }
  }

  /**
   * * ( num ary -- ary )
   *
   * Repeats array given number of times.
   */
  static void w_times(const Ref<Context>& context)
  {
    Ref<Array> array;
    Ref<Number> number;
    std::vector<Ref<Value>> result;

    if (!context->PopArray(array) || !context->PopNumber(number))
    {
      return;
    }
    else if (number->GetNumberType() == Number::NUMBER_TYPE_INT)
    {
      std::int64_t times = number->AsInt();

      result.reserve(array->GetElements().size() * times);
      for (std::int64_t i = 0; i < times; ++i)
      {
        result.insert(
          end(result),
          begin(array->GetElements()),
          end(array->GetElements())
        );
      }
    } else {
      const mpz_class times = number->AsBigInt();

      for (mpz_class i = 0; i < times; ++i)
      {
        result.insert(
          end(result),
          begin(array->GetElements()),
          end(array->GetElements())
        );
      }
    }
    context->PushArray(result);
  }

  Ref<Object> make_array_prototype(Runtime* runtime)
  {
    return runtime->NewPrototype({
      { "len", w_len },

      { "empty?", w_is_empty },
      { "every?", w_every },
      { "some?", w_some },

      { "index-of", w_index_of },
      { "join", w_join },

      { "for-each", w_for_each },
      { "filter", w_filter },
      { "map", w_map },
      { "reduce", w_reduce },
      { "find", w_find },

      { "reverse", w_reverse },
      { "extract", w_extract },

      { "@", w_get },
      { "!", w_set },

      { "+", w_plus },
      { "*", w_times },
    });
  }
}