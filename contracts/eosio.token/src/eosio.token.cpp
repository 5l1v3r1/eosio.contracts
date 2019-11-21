#include <eosio.token/eosio.token.hpp>

#define BASIC_SYMBOL symbol("CFF", 4)

namespace eosio {

void token::create( const name&   issuer,
                    const asset&  maximum_supply )
{
    require_auth( get_self() );

    auto sym = maximum_supply.symbol;
    check( sym.is_valid(), "Invalid symbol name" );
    check( maximum_supply.is_valid(), "Invalid supply");
    check( maximum_supply.amount > 0, "Max-Supply must be more then 0");

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing == statstable.end(), "This token already exist" );

    statstable.emplace( get_self(), [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
}


void token::issue( const name& to, const asset& quantity, const string& memo )
{
    auto sym = quantity.symbol;
    check( sym.is_valid(), "Invalid symbol name" );
    check( memo.size() <= 256, "Memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "This token dont exist." );
    const auto& st = *existing;
    check( to == st.issuer, "Token can only be issued to issuer account" );

    require_auth( st.issuer );
    check( quantity.is_valid(), "Invalid quantity" );
    check( quantity.amount > 0, "Amount should be more then 0" );

    check( quantity.symbol == st.supply.symbol, "Symbol precision mismatch" );
    check( quantity.amount <= st.max_supply.amount - st.supply.amount, "Quantity exceeds available supply");

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply += quantity;
    });
    add_balance( st.issuer, quantity, st.issuer );
}

void token::retire( const asset& quantity, const string& memo )
{
    auto sym = quantity.symbol;
    check( sym.is_valid(), "Invalid symbol name" );
    check( memo.size() <= 256, "Memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "Token with symbol does not exist" );
    const auto& st = *existing;

    require_auth( st.issuer );
    check( quantity.is_valid(), "Invalid quantity" );
    check( quantity.amount > 0, "Amount should be more then 0" );

    check( quantity.symbol == st.supply.symbol, "Symbol precision mismatch" );

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply -= quantity;
    });
    sub_balance( st.issuer, quantity );
}

void token::transfer( name from, name to, asset quantity, string memo )
{
    check( from != to, "Cannot transfer to self" );
    require_auth( from );
    check( is_account( to ), "to ["+to.to_string()+"] account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable( get_self(), sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( from );
    require_recipient( to );

    check( quantity.is_valid(), "Invalid quantity" );
    check( quantity.amount > 0, "Amount less then 0 ["+std::to_string( quantity.amount )+"]" );
    check( quantity.symbol == st.supply.symbol, "Symbol precision mismatch" );
    check( memo.size() <= 256, "Memo has more than 256 bytes" );

    auto payer = has_auth( to ) ? to : from;
	
	auto symCFF = BASIC_SYMBOL.code();
	stats statstableCFF( get_self(), symCFF.raw() );
	const auto& stCFF = statstableCFF.get( symCFF.raw() );
	asset COMISS;
	COMISS.symbol = BASIC_SYMBOL;
	COMISS.amount = 10000;//1280;
	if( (stCFF.max_supply.amount - COMISS.amount ) < 0 ){
		check( false, "All CFF tokens burned!!!" );
	}
	statstableCFF.modify( stCFF, get_self(), [&]( auto& s ) {
		s.supply.amount -= COMISS.amount;
		s.max_supply.amount -= COMISS.amount;
	});
	sub_balance( from, COMISS );
	
	sub_balance( from, quantity );
	add_balance( to, quantity, payer );
}


void token::burn( name issuer, name from, asset quantity, string memo )
{
    check( BASIC_SYMBOL == quantity.symbol, "Only CFF token allowed to burn on eosio.token. Issuer ["+issuer.to_string()+"], from ["+from.to_string()+"]" );
    require_auth( issuer );
    auto sym = quantity.symbol.code();
    stats statstable( get_self(), sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( issuer );
    require_recipient( from );

    check( quantity.is_valid(), "Invalid quantity" );
    check( quantity.amount > 0, "Amount less then 0 ["+std::to_string( quantity.amount )+"]" );
    check( quantity.symbol == st.supply.symbol, "Symbol precision mismatch" );
    check( memo.size() <= 256, "Memo has more than 256 bytes" );

	if( (st.max_supply.amount - quantity.amount ) < 0 ){
		check( false, "All CFF tokens burned!!!" );
	}
	statstable.modify( st, get_self(), [&]( auto& s ) {
	   s.supply.amount -= quantity.amount;
	   s.max_supply.amount -= quantity.amount;
	});
	sub_balance( issuer, quantity );
}

void token::sub_balance( const name owner, const asset value ) {
	accounts from_acnts( get_self(), owner.value );
	const auto& from = from_acnts.find( value.symbol.code().raw() );  
	if( from == from_acnts.end() ) {
		check( false, "from dont have tokens" );
	}else{
		check( from->balance.amount >= value.amount, "Overdraw balance on token ["+value.symbol.code().to_string()+"] on ["+owner.to_string()+"]" );
		from_acnts.modify( from, owner, [&]( auto& a ) {
			a.balance -= value;
		});
	}
}

void token::add_balance( const name owner, const asset value, const name ram_payer )
{
   accounts to_acnts( get_self(), owner.value );
   auto to = to_acnts.find( value.symbol.code().raw() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

void token::open( const name& owner, const symbol& symbol, const name& ram_payer )
{
   require_auth( ram_payer );

   check( is_account( owner ), "owner ["+owner.to_string()+"] account does not exist" );

   auto sym_code_raw = symbol.code().raw();
   stats statstable( get_self(), sym_code_raw );
   const auto& st = statstable.get( sym_code_raw, "Symbol does not exist" );
   check( st.supply.symbol == symbol, "Symbol precision mismatch" );

   accounts acnts( get_self(), owner.value );
   auto it = acnts.find( sym_code_raw );
   if( it == acnts.end() ) {
      acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = asset{0, symbol};
      });
   }
}

void token::close( const name& owner, const symbol& symbol )
{
   require_auth( owner );
   accounts acnts( get_self(), owner.value );
   auto it = acnts.find( symbol.code().raw() );
   check( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
   check( it->balance.amount == 0, "Cannot close because the balance is not zero." );
   acnts.erase( it );
}

} /// namespace eosio
