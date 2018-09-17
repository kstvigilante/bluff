#include "bluff.hpp"

void bluff :: creategame(account_name player_a){

    require_auth(player_a);
    
    asset entry_fee = asset(10000, symbol_type(S(4, SYS)));

    action(
    permission_level{ player_a, N(active) },
    N(eosio.token), N(transfer),
    make_tuple(player_a, _self, entry_fee, string("player 1 created a game")))
    .send();

    game_table gt(_self, _self);
    gt.emplace(_self, [&](auto &g){
        g.player_a = player_a;
        g.id = gt.available_primary_key();
        g.game_status = CREATED;
        g.pot += entry_fee;
        g.total_bet_player_a += entry_fee;
    });

    print("Sending notification");
    require_recipient(player_a);
}

void bluff :: joingame(uint64_t id, account_name player_b){

    require_auth(player_b);
    game_table gt(_self, _self);
    auto itr = gt.find(id);
    eosio_assert(itr != gt.end(), "No such game exists");
    eosio_assert(itr->game_status == CREATED, "Cant join this game");
    eosio_assert(itr->player_a != player_b, "Cannot challenge yourself");
    
    asset joining_fee = asset(10000, symbol_type(S(4, SYS)));
    
    action(
    permission_level{player_b, N(active)},
    N(eosio.token), N(transfer),
    make_tuple(player_b, _self, joining_fee, string("player 2 has joined the game")))
    .send();

    gt.modify(itr, _self, [&](auto &g){
        g.player_b = player_b;
        g.game_status = JOINED;
        g.pot += joining_fee;
        g.total_bet_player_b += joining_fee;
    });

    print("Sending notification");
    require_recipient(player_b);

    startgame(id);
}

void bluff :: fold(uint64_t id, account_name player){

    require_auth(player);
    game_table gt(_self, _self);
    auto itr = gt.find(id);
    eosio_assert(itr->game_status == STARTED, "Cannot fold yet");
    eosio_assert(player == itr->turn, "Not your turn");

    if(player == itr->player_a){
        gt.modify(itr, _self, [&](auto &g){
            g.option_player_a = FOLD;
            g.turn = itr->player_b;
            g.winner = itr->player_b;
            g.game_status = ENDED;
        });
    }
    else{
        gt.modify(itr, _self, [&](auto &g){
            g.option_player_b = FOLD;
            g.turn = itr->player_a;
            g.winner = itr->player_a;
            g.game_status = ENDED;
        });
    }

    decidewinner(id);
}

void bluff :: bet(uint64_t id, account_name player, asset amount){
    
    require_auth(player);
    game_table gt(_self, _self);
    auto itr = gt.find(id);
    eosio_assert(player == itr->turn, "Not your turn yet");
    eosio_assert(itr->game_status == STARTED, "Game has not started yet");

    if(player == itr->player_a){
        
        gt.modify(itr, _self, [&](auto &g){
            g.turn = itr->player_b;
            g.curr_bet_Player_a = amount;
            g.total_bet_player_a += amount;
            g.pot += amount;
            g.option_player_a = BET;
        });

        action(
        permission_level{ player, N(active) },
        N(eosio.token), N(transfer),
        make_tuple(player, _self, amount, string("bet accepted")))
        .send();

    }
    else{
        
        gt.modify(itr, _self, [&](auto &g){
            g.turn = itr->player_a;
            g.curr_bet_player_b = amount;
            g.total_bet_player_b += amount;
            g.pot += amount;
            g.option_player_b = BET;
        });

        action(
        permission_level{ player, N(active) },
        N(eosio.token), N(transfer),
        make_tuple(player, _self, amount, string("bet accepted")))
        .send();
    }
}

void bluff :: accept(uint64_t id, account_name player){

    require_auth(player);
    game_table gt(_self, _self);
    auto itr = gt.find(id);
    eosio_assert(player == itr->turn, "Not your turn yet");
    eosio_assert(itr->game_status == STARTED, "Game has not started yet");

    asset amount;

    if(player == itr->player_a){
        
        amount = itr->curr_bet_player_b;
        gt.modify(itr, _self, [&](auto &g){
            g.turn = itr->player_b;
            g.curr_bet_Player_a = amount;
            g.total_bet_player_a += amount;
            g.pot += amount;
            g.option_player_a = ACCEPT;
        });

        action(
        permission_level{ player, N(active) },
        N(eosio.token), N(transfer),
        make_tuple(player, _self, amount, string("bet accepted")))
        .send();
    }
    else{

        amount = itr->curr_bet_Player_a;
        gt.modify(itr, _self, [&](auto &g){
            g.turn = itr->player_a;
            g.curr_bet_player_b = amount;
            g.total_bet_player_b += amount;
            g.pot += amount;
            g.option_player_b = ACCEPT;
        });

        action(
        permission_level{ player, N(active) },
        N(eosio.token), N(transfer),
        make_tuple(player, _self, amount, string("bet accepted")))
        .send();
    }
}
