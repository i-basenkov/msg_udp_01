#ifndef MSGUTILS_H
#define MSGUTILS_H

#include <variant>

namespace msg
{

	template <typename _Tp, typename... _Rest>
	struct t_index : std::integral_constant<size_t, 0> {};

	template <typename _Tp, typename _First, typename... _Rest>
	struct t_index<_Tp, _First, _Rest...> :
		std::integral_constant<size_t, std::is_same_v<_Tp, _First>
		? 0 : t_index<_Tp, _Rest...>::value + 1> {};

	template <typename T, typename... Ts>
	constexpr
	std::size_t t_index_v = t_index<T, Ts...>::value;



	template <typename T, typename... Ts>
	using has_in_pack = std::disjunction<std::is_same<T, Ts>...>;

	template <typename T, typename... Ts>
	constexpr
	bool has_in_pack_v = has_in_pack<T, Ts...>::value;

	template <typename T, typename... Ts>
	using has_in_pack_b = std::enable_if_t<has_in_pack_v<T, Ts...>, bool>;



	template <typename V, typename = std::void_t<>>
	struct is_variant : std::false_type {};

	template <template <typename...> typename V, typename... T>
	struct is_variant<V<T...>
		, std::enable_if_t<std::is_same_v<std::decay_t<V<T...>>, std::variant<T...>>>
	> : std::true_type {};

	template <typename V>
	constexpr
	bool is_variant_v = is_variant<V>::value;
	
	template <typename V>
	using is_variant_b = std::enable_if_t<is_variant_v<V>, bool>;



	template <typename O, typename = std::void_t<>>
	struct is_optional : std::false_type {};

	template <template <typename...> typename O, typename T>
	struct is_optional<O<T>
		, std::enable_if_t<std::is_same_v<std::decay_t<O<T>>, std::optional<T>>>
	> : std::true_type {};

	template <typename O>
	constexpr
	bool is_optional_v = is_optional<O>::value;
	
	template <typename O>
	using is_optional_b = std::enable_if_t<is_optional_v<O>, bool>;



	template <typename, typename = std::void_t<>>
	struct has_data : std::false_type {};

	template <typename T>
	struct has_data<T
		, std::enable_if_t<!std::is_same_v<decltype(std::declval<T>().data), void>>
	> : std::true_type {};

	template <typename T>
	constexpr
	bool has_data_v = has_data<T>::value;

	template <typename T>
	using has_data_b = std::enable_if_t<has_data_v<T>, bool>;


	template <typename, typename = std::void_t<>>
	struct has_data_f : std::false_type {};

	template <typename T>
	struct has_data_f<T
		, std::enable_if_t<!std::is_same_v<decltype(std::declval<T>().data()), void>>
	> : std::true_type {};

	template <typename T>
	constexpr
	bool has_data_f_v = has_data_f<T>::value;

	template <typename T>
	using has_data_f_b = std::enable_if_t<has_data_f_v<T>, bool>;





	namespace _detale
	{
		template <typename R>
		struct __to_t
		{
			__to_t() = default;
			__to_t(__to_t const&) = delete;
			__to_t(__to_t&&) = delete;
			__to_t& operator=(__to_t const&) = delete;
			__to_t& operator=(__to_t&&) = delete;

			// Привести значение std::variant к типу
			template <template <typename...> typename V, typename... T
				, is_variant_b<V<T...>> = true
				, has_in_pack_b<R, T...> = true
			>
			constexpr
			auto operator()(V<T...> const& v) const noexcept
			-> std::optional<std::reference_wrapper<const R>>
			{
				if (auto tv = std::get_if<R>(&v); tv)
					return std::make_optional(std::ref(*tv));
				else
					return std::nullopt;
			}			
		};
	}
	template <typename R>
	constexpr
	_detale::__to_t<R> to;

	// Привести значение std::variant к типу
	template <template<typename> typename V, typename T, typename... Ts
		, is_variant_b<V<Ts...>> = true
		, has_in_pack_b<T, Ts...> = true
	>
	constexpr
	auto operator | (V<Ts...> const& v, _detale::__to_t<T> const& f) noexcept
	-> std::optional<std::reference_wrapper<const T>>
	{
		return f(v);
	}



	namespace _detale
	{
		template <typename F>
		struct __into_t
		{
			__into_t() = delete;
			__into_t(__into_t const&) = delete;
			__into_t(__into_t&&) = delete;
			__into_t& operator=(__into_t const&) = delete;
			__into_t& operator=(__into_t&&) = delete;

			constexpr
			__into_t(F const& _f) noexcept : m_func{_f} {}

			// Передать в функцию
			template <typename D
				, std::enable_if_t<std::is_invocable_v<F const&, D const&>, bool> = true
			>
			constexpr
			void operator()(D const& d) const noexcept
			{
				m_func(d);
			}

		private:
			F const& m_func;
		};

		struct __create_into_t
		{
			__create_into_t() = default;
			__create_into_t(__create_into_t const&) = delete;
			__create_into_t(__create_into_t&&) = delete;
			__create_into_t& operator=(__create_into_t const&) = delete;
			__create_into_t& operator=(__create_into_t&&) = delete;

			// Передать в функцию
			template <typename F>
			constexpr
			auto operator()(F&& _f) const noexcept
			{
				return __into_t<F>(std::forward<F>(_f));
			}
			// Передать в функцию
			template <typename F>
			constexpr
			auto operator >> (F&& _f) const noexcept
			{
				return __into_t<F>(std::forward<F>(_f));
			}
		};
	}
	// Передать в функцию
	constexpr
	_detale::__create_into_t into;

	// Передать в функцию
	template <typename F, template<typename> typename O, typename D
		, is_optional_b<O<std::reference_wrapper<D const>>> = true
		, std::enable_if_t<std::is_invocable_v<F const&, D const&>, bool> = true
	>
	constexpr
	auto operator | (O<std::reference_wrapper<D const>> d
					, _detale::__into_t<F> const& _f) noexcept
	{
		if (d) _f(d.value().get());
		return d;
	}

	// Передать в функцию
	template <typename F, template<typename> typename O, typename D
		, is_optional_b<O<std::reference_wrapper<D>>> = true
		, std::enable_if_t<std::is_invocable_v<F const&, D&>, bool> = true
	>
	constexpr
	auto operator | (O<std::reference_wrapper<D>> d
					, _detale::__into_t<F> const& _f) noexcept
	{
		if (d) _f(d.value().get());
		return d;
	}


	namespace _detale
	{
		template <typename D>
		struct __copy_t
		{
			__copy_t() = delete;
			__copy_t(__copy_t const&) = delete;
			__copy_t(__copy_t&&) = delete;
			__copy_t& operator=(__copy_t const&) = delete;
			__copy_t& operator=(__copy_t&&) = delete;

			constexpr
			__copy_t(D& _d) noexcept : m_data{_d} {}

			// Копировать в
			template <typename _T
				, std::enable_if_t<std::is_constructible_v<std::decay_t<D>, std::decay_t<_T>>, bool> = true
			>
			void operator()(_T const& _d) const noexcept
			{
				m_data = static_cast<D>(_d);
			}

		private:
			D& m_data;
		};

		struct __create_copy_t
		{
			__create_copy_t() = default;
			__create_copy_t(__create_copy_t const&) = delete;
			__create_copy_t(__create_copy_t&&) = delete;
			__create_copy_t& operator=(__create_copy_t const&) = delete;
			__create_copy_t& operator=(__create_copy_t&&) = delete;

			// Копировать в
			template <typename _D>
			constexpr
			auto operator()(_D& _d) const noexcept
			{
				return __copy_t(_d);
			}

			// Копировать в
			template <typename _D>
			constexpr
			auto operator >> (_D& _d) const noexcept
			{
				return __copy_t(_d);
			}
		};
	}
	// Копировать в
	constexpr
	_detale::__create_copy_t copy;

	// Копировать в
	template <typename DD, template<typename> typename O, typename SD
		, is_optional_b<O<std::reference_wrapper<SD const>>> = true
		, std::enable_if_t<std::is_constructible_v<std::decay_t<DD>, std::decay_t<SD>>, bool> = true
	>
	constexpr
	auto operator | (O<std::reference_wrapper<SD const>> d
					, _detale::__copy_t<DD> const& _f) noexcept
	{
		if (d) _f(d.value().get());
		return d;
	}



	namespace _detale
	{
		struct _data_t
		{
			_data_t() = default;
			_data_t(_data_t const&) = delete;
			_data_t(_data_t&&) = delete;
			_data_t& operator=(_data_t const&) = delete;
			_data_t& operator=(_data_t&&) = delete;

			template <typename D
				, has_data_b<D> = true
			>
			constexpr
			auto operator()(D const& d) const noexcept
			{
				return std::make_optional(std::ref(d.data));
			}
			template <typename D
				, has_data_f_b<D> = true
			>
			constexpr
			auto operator()(D const& d) const noexcept
			{
				return std::make_optional(std::ref(d.data()));
			}
		};
	}
	// Возвратить поле data
	constexpr
	_detale::_data_t data;

	// Возвратить поле data
	template <typename D
		, has_data_b<D> = true
	>
	constexpr
	auto operator | (D&& d, _detale::_data_t const& _f) noexcept
	-> std::optional<std::reference_wrapper<const std::decay_t<decltype(std::declval<D>().data)>>>
	{
		if (d)
			return _f(std::forward<D>(d));
		else
			return std::nullopt;
	}

	// Возвратить поле data
	template <template<typename> typename O, typename D
		, is_optional_b<O<std::reference_wrapper<D const>>> = true
		, has_data_b<D> = true
	>
	constexpr
	auto operator | (O<std::reference_wrapper<D const>> d
							, _detale::_data_t const& _f) noexcept
	-> std::optional<std::reference_wrapper<const std::decay_t<decltype(std::declval<D>().data)>>>
	{
		if (d)
			return _f(d.value().get());
		else
			return std::nullopt;
	}

	// Возвратить поле data
	template <typename D
		, has_data_f_b<D> = true
	>
	constexpr
	auto operator | (D&& d, _detale::_data_t const& _f) noexcept
	-> std::optional<std::reference_wrapper<const std::decay_t<decltype(std::declval<D>().data())>>>
	{
		if (d)
			return _f(std::forward<D>(d));
		else
			return std::nullopt;
	}

	// Возвратить поле data
	template <template<typename> typename O, typename D
		, is_optional_b<O<std::reference_wrapper<D const>>> = true
		, has_data_f_b<D> = true
	>
	constexpr
	auto operator | (O<std::reference_wrapper<D const>> d
							, _detale::_data_t const& _f) noexcept
	-> std::optional<std::reference_wrapper<const std::decay_t<decltype(std::declval<D>().data())>>>
	{
		if (d)
			return _f(d.value().get());
		else
			return std::nullopt;
	}



	namespace _detale
	{
		template <typename F>
		struct __if_error_t
		{
			__if_error_t() = delete;
			__if_error_t(__if_error_t const&) = delete;
			__if_error_t(__if_error_t&&) = delete;
			__if_error_t& operator=(__if_error_t const&) = delete;
			__if_error_t& operator=(__if_error_t&&) = delete;

			constexpr
			__if_error_t(F const& _f) noexcept : m_func{_f} {}

			//Если ошибка вызвать функцию
			constexpr
			void operator()() const noexcept
			{
				m_func();
			}

		private:
			F const& m_func;
		};

		struct __create_if_error_t
		{
			__create_if_error_t() = default;
			__create_if_error_t(__create_if_error_t const&) = delete;
			__create_if_error_t(__create_if_error_t&&) = delete;
			__create_if_error_t& operator=(__create_if_error_t const&) = delete;
			__create_if_error_t& operator=(__create_if_error_t&&) = delete;

			//Если ошибка вызвать функцию
			template <typename F>
			constexpr
			auto operator()(F&& _f) const noexcept
			{
				return __if_error_t<F>(std::forward<F>(_f));
			}
			//Если ошибка вызвать функцию
			template <typename F>
			constexpr
			auto operator >> (F&& _f) const noexcept
			{
				return __if_error_t<F>(std::forward<F>(_f));
			}
		};
	}
	//Если ошибка вызвать функцию
	constexpr
	_detale::__create_if_error_t if_error;

	//Если ошибка вызвать функцию
	template <typename F, template<typename> typename O, typename D
		, is_optional_b<O<std::reference_wrapper<D>>> = true
	>
	constexpr
	auto operator | (O<std::reference_wrapper<D>> od
					, _detale::__if_error_t<F> const& _f) noexcept
	{
		if (!od) _f();
		return od;
	}


} // namespace msg

#endif // MSGUTILS_H