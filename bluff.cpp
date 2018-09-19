#include "bluff.hpp"

void bluff :: creategame(account_name player){

    require_auth(player);
    
    asset entry_fee = asset(10000, symbol_type(S(4, SYS)));

    depositfunds(player, _self, entry_fee, "Player1  created a game");

    game_table gt(_self, _self);
    gt.emplace(_self, [&](auto &g){
        g.player_a = player;
        g.id = gt.available_primary_key();
        g.game_status = CREATED;
        g.pot += entry_fee;
        g.total_bet_player_a += entry_fee;
    });

    print("Sending notification");
    require_recipient(player);
}

void bluff :: joingame(uint64_t id, account_name player){

    require_auth(player);
    game_table gt(_self, _self);
    auto itr = gt.find(id);
    eosio_assert(itr != gt.end(), "No such game exists");
    eosio_assert(itr->game_status == CREATED, "Cant join this game");
    eosio_assert(itr->player_a != player, "Cannot challenge yourself");
    
    asset joining_fee = asset(10000, symbol_type(S(4, SYS)));
    
    depositfunds(player, _self, joining_fee, "Player2 has joined the game");

    gt.modify(itr, _self, [&](auto &g){
        g.player_b = player;
        g.game_status = JOINED;
        g.pot += joining_fee;
        g.total_bet_player_b += joining_fee;
    });

    print("Sending notification");
    require_recipient(player);

    startgame(id);
}

void bluff :: fold(uint64_t id, account_name player){

    require_auth(player);
    game_table gt(_self, _self);
    auto itr = gt.find(id);
    eosio_assert(itr != gt.end(), "No such game exists");
    eosio_assert(itr->game_status == STARTED, "Cannot fold yet");
    eosio_assert(player == itr->turn, "Not your turn");

    if(player == itr->player_a){
        gt.modify(itr, _self, [&](auto &g){
            g.turn = itr->player_b;
            g.option_player_a = FOLD;
            g.winner = itr->player_b;
            g.game_status = ENDED;
        });
    }
    else{
        gt.modify(itr, _self, [&](auto &g){
            g.turn = itr->player_a;
            g.option_player_b = FOLD;
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
    eosio_assert(itr != gt.end(), "No such game exists");
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

    depositfunds(player, _self, amount, "bet accepted");

    }
    else{
        
        gt.modify(itr, _self, [&](auto &g){
            g.turn = itr->player_a;
            g.curr_bet_player_b = amount;
            g.total_bet_player_b += amount;
            g.pot += amount;
            g.option_player_b = BET;
        });

    depositfunds(player, _self, amount, "bet accepted");

    }
    print("End of bet function");
}

void bluff :: accept(uint64_t id, account_name player){

    require_auth(player);
    game_table gt(_self, _self);
    auto itr = gt.find(id);
    eosio_assert(itr != gt.end(), "Game doesnt exist");
    eosio_assert(player == itr->turn, "Not your turn");
    eosio_assert(itr->game_status == STARTED, "Game has not started yet");

    if(player == itr->player_a){
        
        asset amount;
        if(itr->option_player_b == BET){
            
            amount = itr->curr_bet_player_b;
            depositfunds(player, _self, amount, "Player1 accepted the bet");
            gt.modify(itr, _self, [&](auto &g){
                g.turn = itr->player_b;
                g.curr_bet_Player_a = amount;
                g.total_bet_player_a += amount;
                g.pot += amount;
                g.option_player_a = ACCEPT;
            });
        }
        if(itr->option_player_b == RAISE){
            
            amount = itr->curr_bet_player_b - itr->curr_bet_Player_a;
            depositfunds(player, _self, amount, "Player1 accepted the bet");
            gt.modify(itr, _self, [&](auto &g){
                g.turn = itr->player_b;
                g.curr_bet_Player_a += amount;
                g.total_bet_player_a += amount;
                g.pot += amount;
                g.option_player_a = ACCEPT;
            });
        }
    }
    else{

        asset amount;

        if(itr->option_player_a == BET){
            
            amount = itr->curr_bet_Player_a;
            depositfunds(player, _self, amount, "Player2 accepted the bet");
            gt.modify(itr, _self, [&](auto &g){
                g.turn = itr->player_a;
                g.curr_bet_player_b = amount;
                g.total_bet_player_b += amount;
                g.pot += amount;
                g.option_player_b = ACCEPT;
            });

        }
        if(itr->option_player_a == RAISE){
            
            amount = itr->curr_bet_Player_a - itr->curr_bet_player_b;
            depositfunds(player, _self, amount, "Player2 accepted the bet");
            gt.modify(itr, _self, [&](auto &g){
                g.turn = itr->player_a;
                g.curr_bet_player_b += amount;
                g.total_bet_player_b += amount;
                g.pot += amount;
                g.option_player_b = ACCEPT;
            });

        }
    }
}

    
void bluff :: raise(uint64_t id, account_name player, asset amount){

    require_auth(player);
    game_table gt(_self, _self);
    auto itr = gt.find(id);

    eosio_assert(itr != gt.end(), "No such game exists");
    eosio_assert(player == itr->turn, "Not your turn ");
    eosio_assert(itr->game_status == STARTED, "Game has not started yet");

    if(player == itr->player_a){

        eosio_assert(amount>itr->curr_bet_player_b, "Amount too low");

        if(itr->option_player_b == BET){
            
            depositfunds(player, _self, amount, "Player1 raised the bet");
            gt.modify(itr, _self, [&](auto &g){
                g.turn = itr->player_b;
                g.curr_bet_Player_a = amount;
                g.total_bet_player_a += amount;
                g.option_player_a = RAISE;
                g.pot += amount;
            });
        }

        if(itr->option_player_b == RAISE){

            amount = amount - itr->curr_bet_Player_a;
            depositfunds(player, _self, amount, "Player1 raised the bet");
            gt.modify(itr, _self, [&](auto &g){
                g.turn = itr->player_b;
                g.curr_bet_Player_a += amount;
                g.total_bet_player_a += amount;
                g.option_player_a = RAISE;
                g.pot += amount;
            });
        }
    }
    else{

        eosio_assert(amount > itr->curr_bet_Player_a, "Amount too low");

        if(itr->option_player_a == BET){

            depositfunds(player, _self, amount, "Player2 raised the bet");
            gt.modify(itr, _self, [&](auto &g){
                g.turn = itr->player_a;
                g.curr_bet_player_b = amount;
                g.total_bet_player_b += amount;
                g.pot += amount;
                g.option_player_b = RAISE;
            });
        }

        if(itr->option_player_a == RAISE){

            amount = amount - itr->curr_bet_player_b;
            depositfunds(player, _self, amount, "Player2 raised");
            gt.modify(itr, _self, [&](auto &g){
                g.turn = itr->player_a;
                g.curr_bet_player_b += amount;
                g.total_bet_player_b += amount;
                g.pot += amount;
                g.option_player_b = RAISE;
            });
        }
    }
}