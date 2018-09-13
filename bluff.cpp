#include "bluff.hpp"

void bluff:: creategame(account_name player_a){

    require_auth(player_a);
    
    asset entry_fee = asset(10000, symbol_type(S(4, SYS)));

    action(
    permission_level{ player_a, N(active) },
    N(eosio.token), N(transfer),
    std::make_tuple(player_a, _self, entry_fee, string("player 1 created a game")))
    .send();
 
    game_table gt(_self, _self);
    gt.emplace(_self, [&](auto &g){
        g.player_a = player_a;
        g.id = gt.available_primary_key();
        g.game_status = CREATED;
        g.pot += entry_fee;
        g.total_bet_player_a += entry_fee;
    });
}

void bluff:: joingame(uint64_t id, account_name player_b){

    require_auth(player_b);
    game_table gt(_self, _self);
    auto itr = gt.find(id);
    eosio_assert(itr != gt.end(), "No such game exists");
    eosio_assert(itr->game_status == CREATED, "Cant join this game");
    
    asset joining_fee = asset(10000, symbol_type(S(4, SYS)));
    
    action(
    permission_level{player_b, N(active)},
    N(eosio.token), N(transfer),
    std::make_tuple(player_b, _self, joining_fee, string("player 2 has joined the game")))
    .send();

    gt.modify(itr, _self, [&](auto &g){
        g.player_b = player_b;
        g.game_status = JOINED;
        g.pot += joining_fee;
        g.total_bet_player_b += joining_fee;
    });

    startgame(id);
}
