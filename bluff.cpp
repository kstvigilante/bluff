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
        g.last_playerb = now();
        g.last_playera = now();
    });

    print("Sending notification");
    require_recipient(player);

    startgame(id);
    createcard(id);
}

void bluff :: fold(uint64_t id, account_name player){

    require_auth(player);
    game_table gt(_self, _self);
    auto itr = gt.find(id);
    eosio_assert(itr != gt.end(), "No such game exists");
    eosio_assert(itr->game_status == STARTED, "Cannot fold yet");
    eosio_assert(player == itr->turn, "Not your turn");
    eosio_assert(player == itr->player_a || player == itr->player_b, "Not part of this game");

    if(player == itr->player_a){
        gt.modify(itr, _self, [&](auto &g){
            g.turn = itr->player_b;
            g.option_player_a = FOLD;
            g.winner = itr->player_b;
            g.game_status = ENDED;
            g.last_playera = now();
        });
    }
    else{
        gt.modify(itr, _self, [&](auto &g){
            g.turn = itr->player_a;
            g.option_player_b = FOLD;
            g.winner = itr->player_a;
            g.game_status = ENDED;
            g.last_playerb = now();
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
    eosio_assert(player == itr->player_a || player == itr->player_b, "Not part of this game");

    if(player == itr->player_a){
        
        gt.modify(itr, _self, [&](auto &g){
            g.turn = itr->player_b;
            g.curr_bet_Player_a = amount;
            g.total_bet_player_a += amount;
            g.pot += amount;
            g.option_player_a = BET;
            g.last_playera = now();
        });

    depositfunds(player, _self, amount, "Player1 made a bet");

    }
    else{
        
        gt.modify(itr, _self, [&](auto &g){
            g.turn = itr->player_a;
            g.curr_bet_player_b = amount;
            g.total_bet_player_b += amount;
            g.pot += amount;
            g.option_player_b = BET;
            g.last_playerb = now();
        });

    depositfunds(player, _self, amount, "Player2 made a bet");

    }
}

void bluff :: accept(uint64_t id, account_name player){

    require_auth(player);
    game_table gt(_self, _self);
    auto itr = gt.find(id);
    eosio_assert(itr != gt.end(), "Game doesnt exist");
    eosio_assert(player == itr->turn, "Not your turn");
    eosio_assert(itr->game_status == STARTED, "Game has not started yet");
    eosio_assert(player == itr->player_a || player == itr->player_b, "Not part of this game");

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
                g.last_playera = now();
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
                g.last_playera = now();
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
                g.last_playerb = now();
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
                g.last_playerb = now();
            });

        }
    }
    nextround(id);
    createcard(id);
}

    
void bluff :: raise(uint64_t id, account_name player, asset amount){

    require_auth(player);
    game_table gt(_self, _self);
    auto itr = gt.find(id);

    eosio_assert(itr != gt.end(), "No such game exists");
    eosio_assert(player == itr->turn, "Not your turn");
    eosio_assert(itr->game_status == STARTED, "Game has not started yet");
    eosio_assert(player == itr->player_a || player == itr->player_b, "Not part of this game");

    if(player == itr->player_a){

        eosio_assert(amount>itr->curr_bet_player_b, "Amount too low");
        eosio_assert(itr->option_player_b != NONE, "Cannot raise now");
        eosio_assert(itr->option_player_b != HOLD, "Cannot raise now");

        if(itr->option_player_b == BET){
            
            depositfunds(player, _self, amount, "Player1 raised the bet");
            gt.modify(itr, _self, [&](auto &g){
                g.turn = itr->player_b;
                g.curr_bet_Player_a = amount;
                g.total_bet_player_a += amount;
                g.option_player_a = RAISE;
                g.pot += amount;
                g.last_playera = now();
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
                g.last_playera = now();
            });
        }
    }
    else{

        eosio_assert(amount > itr->curr_bet_Player_a, "Amount too low");
        eosio_assert(itr->option_player_a != NONE, "Cannot raise now");
        eosio_assert(itr->option_player_a != HOLD, "Cannot raise now");

        if(itr->option_player_a == BET){

            depositfunds(player, _self, amount, "Player2 raised the bet");
            gt.modify(itr, _self, [&](auto &g){
                g.turn = itr->player_a;
                g.curr_bet_player_b = amount;
                g.total_bet_player_b += amount;
                g.pot += amount;
                g.option_player_b = RAISE;
                g.last_playerb = now();
            });
        }

        if(itr->option_player_a == RAISE){

            amount = amount - itr->curr_bet_player_b;
            depositfunds(player, _self, amount, "Player2 raised the bet");
            gt.modify(itr, _self, [&](auto &g){
                g.turn = itr->player_a;
                g.curr_bet_player_b += amount;
                g.total_bet_player_b += amount;
                g.pot += amount;
                g.option_player_b = RAISE;
                g.last_playerb = now();
            });
        }
    }
}

void bluff :: hold(uint64_t id, account_name player){

    require_auth(player);
    game_table gt(_self, _self);
    auto itr = gt.find(id);

    eosio_assert(itr != gt.end(), "No such game exist hold");
    eosio_assert(player == itr->turn, "Not your turn hold");
    eosio_assert(itr->game_status == STARTED, "Game has not started yet hold");
    eosio_assert(player == itr->player_a || player == itr->player_b, "Not part of this game");

    if(player == itr->player_a){

        if(itr->option_player_b == NONE){

            gt.modify(itr, _self,[&](auto &g){
                g.turn = itr->player_b;
                g.option_player_a = HOLD;
                g.last_playera = now();
            });
            print("Player1 did not place a bet");
        }

        else if(itr->option_player_b == HOLD){
            
            gt.modify(itr, _self,[&](auto &g){
                g.turn = itr->player_b;
                g.option_player_a = HOLD;
                g.last_playera = now();
            });
            print("Player1 did not place a bet");
            nextround(id);
            createcard(id);
        }

        else{
            
            gt.modify(itr, _self,[&](auto &g){
                g.option_player_a = HOLD;
                g.last_playera = now();
            });
            fold(id, player);
        }
    }
    else{

        if(itr->option_player_a == NONE){

            gt.modify(itr, _self, [&](auto &g){
                g.turn = itr->player_a;
                g.option_player_b = HOLD;
                g.last_playerb = now();
            });   
            print("Player2 did not place a bet");
        }

        else if(itr->option_player_a == HOLD){
            
            gt.modify(itr, _self,[&](auto &g){
                g.turn = itr->player_a;
                g.option_player_b = HOLD;
                g.last_playerb = now();
            });
            print("Player2 did not place a bet");
            nextround(id);
            createcard(id);
        }

        else{

            gt.modify(itr, _self,[&](auto &g){
                g.option_player_b = HOLD;
                g.last_playerb = now();
            });
            fold(id, player);
        }
    } 
}

void bluff :: claimwinner(uint64_t id, account_name player){

    require_auth(player);
    game_table gt(_self, _self);
    auto itr = gt.find(id);

    eosio_assert(itr != gt.end(), "No such game exists");
    eosio_assert(itr->game_status == STARTED, "Game has not started yet");
    eosio_assert(player != itr->turn, "Cannot call when it is your turn");
    eosio_assert(player == itr->player_a || player == itr->player_b, "Not part of this game");
    time last = now();
    
    if(player == itr->player_a){

        eosio_assert(last > itr->last_playera + 60, "1 minute is not over");    
        gt.modify(itr, _self, [&](auto &g){
            g.winner = itr->player_a;
            g.game_status = ENDED;
        });
        decidewinner(id);
    }
    else{
        
        eosio_assert(last > itr->last_playerb + 60, "1 minute has not passed yet");
        gt.modify(itr, _self,[&](auto &g){
            g.winner = itr->player_b;
            g.game_status = ENDED;
        });
        decidewinner(id);
    }
}
