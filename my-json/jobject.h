//
// Created by Alone on 2022-7-25.
//

#ifndef MYUTIL_JOBJECT_H
#define MYUTIL_JOBJECT_H

#include "noncopyable.h"
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace json
{
	using std::get_if;
	using std::map;
	using std::string;
	using std::string_view;
	using std::stringstream;
	using std::variant;
	using std::vector;

	enum TYPE
	{
		T_NULL, T_BOOL, T_INT, T_DOUBLE, T_STR, T_LIST, T_DICT
	};

	class JObject;

	using null_t = bool;
	using int_t = int64_t;
	using bool_t = bool;
	using double_t = double;
	using str_t = string_view;
	using list_t = vector<JObject>;
	using dict_t = map<str_t, JObject>;

#define IS_TYPE(typea, typeb) std::is_same<typea, typeb>::value

	template<class T> using decay = typename std::decay<T>::type;
	template<class T>
	constexpr bool is_number()
	{
		if constexpr (std::is_floating_point_v<T> || std::is_integral_v<T>)
			return true;

		return false;
	}

	template<class T>
	constexpr bool is_basic_type()
	{
		if constexpr (IS_TYPE(T, str_t) || IS_TYPE(T, bool_t) ||
			IS_TYPE(T, int_t) || is_number<T>())
			return true;

		return false;
	}

	struct number
	{
		union
		{
			double_t d_;
			int_t i_;
		};
		template<class T, typename std::enable_if<std::is_integral_v<decay<T>>, bool>::type = true>
		explicit number(T i):i_(i)
		{
		}

		template<class T, typename std::enable_if<std::is_floating_point_v<decay<T>>, bool>::type = true>
		explicit number(T d):d_(d)
		{
		}
	};

	class JObject : noncopyable
	{
	 public:
		using value_t = variant<bool_t, number, str_t, list_t, dict_t>;

		struct ObjectRef
		{
			JObject& ref;
			template<class T>
			ObjectRef& get_from(T&& src)
			{
				using RawT = decay<T>;
				if constexpr (is_basic_type<RawT>())
				{
					ref = std::move(JObject(std::forward<T>(src)));
				}
				else if constexpr (IS_TYPE(RawT, string))
				{
					ref = JObject(std::forward<T>(src));
				}
				else if constexpr (IS_TYPE(RawT, JObject))
				{
					ref = std::forward<T>(src);
				}
				else
				{
					auto tmp = JObject(dict_t{});
					to_json(tmp, static_cast<const T&>(src));
					ref = std::move(tmp);
				}

				return *this;
			}
			template<class T>
			void get_to(T& dst)
			{
				using RawT = decay<T>;
				if constexpr (is_basic_type<RawT>())
				{
					dst = std::move(ref.Value<RawT>());
				}
				else if constexpr (IS_TYPE(RawT, string))
				{
					dst = ref.Value<str_t>();
				}
				else
				{
					from_json(ref, dst);
				}
			}
		};

		JObject(JObject&&) noexcept = default;
		JObject& operator=(JObject&&) = default;

		JObject() : m_value(false), m_type(T_NULL)
		{
		}

		template<class T,typename std::enable_if<std::is_integral_v<T>,bool>::type = true>
		explicit JObject(T value) : m_value(number{ value }), m_type(T_INT)
		{
		}

		template<class T,typename std::enable_if<std::is_floating_point_v<T>,bool>::type = true>
		explicit JObject(T value) : m_value(number{ value }), m_type(T_DOUBLE)
		{
		}

		explicit JObject(bool_t value) : m_value(value), m_type(T_BOOL)
		{
		}

		explicit JObject(str_t value) : m_value(value), m_type(T_STR)
		{
		}

		explicit JObject(list_t value) : m_value(std::move(value)), m_type(T_LIST)
		{
		}

		explicit JObject(dict_t value) : m_value(std::move(value)), m_type(T_DICT)
		{
		}

#define THROW_GET_ERROR(erron)                                                 \
  throw std::logic_error("type error in get " #erron " value!")

		template<class V>
		[[nodiscard]] V& Value() const
		{
			// 添加安全检查
			if constexpr (IS_TYPE(V, str_t))
			{
				if (m_type != T_STR)
					THROW_GET_ERROR(string);
			}
			else if constexpr (IS_TYPE(V, bool_t))
			{
				if (m_type != T_BOOL)
					THROW_GET_ERROR(BOOL);
			}
			else if constexpr (is_number<V>())
			{
				if (m_type != T_INT && m_type != T_DOUBLE)
					THROW_GET_ERROR(NUMBER);
			}
			else if constexpr (IS_TYPE(V, list_t))
			{
				if (m_type != T_LIST)
					THROW_GET_ERROR(LIST);
			}
			else if constexpr (IS_TYPE(V, dict_t))
			{
				if (m_type != T_DICT)
					THROW_GET_ERROR(DICT);
			}

			void* v = value();
			if (v == nullptr)
				throw std::logic_error("unknown type in JObject::Value()");
			//为了解决number类型之间的互转
			if constexpr (is_number<V>())
			{
				thread_local V local_v;
				if (m_type == T_INT)
				{
					local_v = ((number*)(v))->i_;
				}
				else if (m_type == T_DOUBLE)
				{
					local_v = ((number*)(v))->d_;
				}
				return local_v;
			}

			return *((V*)v);
		}

		TYPE Type()
		{
			return m_type;
		}

		string to_string();

		void push_back(JObject item)
		{
			if (m_type == T_LIST)
			{
				auto& list = Value<list_t>();
				list.push_back(std::move(item));
				return;
			}
			throw std::logic_error("not a list type! JObjcct::push_back()");
		}

		void pop_back()
		{
			if (m_type == T_LIST)
			{
				auto& list = Value<list_t>();
				list.pop_back();
				return;
			}
			throw std::logic_error("not list type! JObjcct::pop_back()");
		}

		[[nodiscard]] ObjectRef at(str_t key) const
		{
			if (m_type == T_DICT)
			{
				auto& dict = Value<dict_t>();
				return ObjectRef{ dict[key] };
			}
			throw std::logic_error("not dict type! JObject::operator[]()");
		}

	 private:
		// 根据类型获取值的地址，直接硬转为void*类型，然后外界调用Value函数进行类型的强转
		[[nodiscard]] void* value() const;

	 private:
		value_t m_value;
		TYPE m_type;
	};
} // namespace json

#endif // MYUTIL_JOBJECT_H
