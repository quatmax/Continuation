#include <iostream>
#include <coroutine>
#include <thread>
#include <functional>

namespace
{
	template< typename R >
	struct [[nodiscard]] Continuation 
	{
		struct promise_type
		{
			Continuation< R > get_return_object() 
			{
				return Continuation< R >{ std::coroutine_handle< promise_type >::from_promise( *this ) };
			}
			struct ResumeContinuation
			{
				ResumeContinuation() noexcept {}
				bool await_ready() const noexcept { return false; }
				void await_suspend( std::coroutine_handle< promise_type > coroutine) noexcept
				{
					auto& promise = coroutine.promise();
					if( promise.continuation_ )
						promise.continuation_.resume();
				}
				void await_resume() noexcept {}
			};
			promise_type() noexcept {}
			auto initial_suspend() noexcept { return std::suspend_never{}; }
			auto final_suspend() noexcept { return ResumeContinuation{}; }
			void return_value( R result ) { result_ = result; }
			void unhandled_exception() {}
			R result_ = {}; 
			std::coroutine_handle<> continuation_;
		};

		Continuation( const Continuation& ) = delete;
		Continuation& operator=( const Continuation& ) = delete;
		Continuation& operator=( Continuation&& ) noexcept = delete;

		Continuation( Continuation&& t ) noexcept { coroutine_(t.coroutine_); }
		explicit Continuation( std::coroutine_handle< promise_type > coroutine ) 
			: coroutine_( coroutine ) 
		{}
		~Continuation()
		{
			if( coroutine_ )
				coroutine_.destroy();
		}

		bool await_ready() const noexcept
		{
			return false;
		}
		void await_suspend( std::coroutine_handle<> awaitingCoroutine ) noexcept
		{
			coroutine_.promise().continuation_ = awaitingCoroutine;
		}
		auto await_resume()
		{
			return coroutine_.promise().result_; 
		}

	private:
		std::coroutine_handle< promise_type > coroutine_;
	};

	template< typename R > using Callback = std::function< void( R ) >;
	template< typename R > using Api = std::function< void( Callback< R > ) >;
	template< typename R >
		struct CallbackContinuationAwaiter
		{
			CallbackContinuationAwaiter( const Api< R > &api )
				:api_( api ) {}

			bool await_ready() { return false; }
			void await_suspend( std::coroutine_handle<> handle )
			{ 
				api_( [ this, handle ]( const R& r ) 
				{ 
					result_ = r;
					handle.resume();
				});
			}
			R await_resume() { return result_; }

			const Api< R > api_;
			R result_ = {};
		};
	template< typename R >
		auto CallbackContinuation( Api< R > api )
		{
			return CallbackContinuationAwaiter< R >{ api };
		}

// TEST
#define BOOST_TEST( x ) \
	if( !( x ) ) \
		std::cout << "Error: " << #x << std::endl;

#define BOOST_TEST_MESSAGE( x ) \
	std::cout << x << std::endl;

	std::thread t;

	void api( const std::function< void( int ) >& callback )
	{
		t = std::thread( [ = ] 
		{ 
			std::this_thread::sleep_for( std::chrono::seconds{ 5 } );
			callback( 41 );
		});
	}

	Continuation< int > Test1X()
	{
		int initalValue = 1;
 		BOOST_TEST_MESSAGE( "Start Test1X" );
		int x = co_await CallbackContinuation< int >( &api );
		x += initalValue;
		BOOST_TEST( x == 42 );
		BOOST_TEST_MESSAGE( "Test1X" );
		co_return x += 1;
	}

	Continuation< double > Test1aX()
	{
		BOOST_TEST_MESSAGE( "Start Test1aX" );
		int x = co_await Test1X();
		BOOST_TEST( x == 43 );
		BOOST_TEST_MESSAGE( "Test1aX" );
		co_return x + 1.0;
	}

	Continuation< nullptr_t > Test2X()
	{
		BOOST_TEST_MESSAGE( "Start Test2X" );
		auto x = co_await Test1aX();
		BOOST_TEST( x == 44.0 );
		BOOST_TEST_MESSAGE( "Test2X" );
		co_return {};
	}
}

int main()
{
	BOOST_TEST_MESSAGE( "main start" );
	auto test = Test2X();
	BOOST_TEST_MESSAGE( "main after Test2X" );
	std::this_thread::sleep_for( std::chrono::seconds{ 1 } );
	BOOST_TEST_MESSAGE( "main after sleep" );
	t.join();
	BOOST_TEST_MESSAGE( "after join" );
}

