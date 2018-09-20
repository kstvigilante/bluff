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
    eosio_assert(itr != gt.end(), "No such game exists joingame");
    eosio_assert(itr->game_status == CREATED, "Cant join this game joingame");
    eosio_assert(itr->player_a != player, "Cannot challenge yourself joingame");
    
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
    createcard(id);
}

void bluff :: fold(uint64_t id, account_name player){

    require_auth(player);
    game_table gt(_self, _self);
    auto itr = gt.find(id);
    eosio_assert(itr != gt.end(), "No such game exists fold");
    eosio_assert(itr->game_status == STARTED, "Cannot fold yet fold");
    eosio_assert(player == itr->turn, "Not your turn fold ");

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
    eosio_assert(itr != gt.end(), "No such game exists bet");
    eosio_assert(player == itr->turn, "Not your turn yet bet");
    eosio_assert(itr->game_status == STARTED, "Game has not started yet bet");

    if(player == itr->player_a){
        
        gt.modify(itr, _self, [&](auto &g){
            g.turn = itr->player_b;
            g.curr_bet_Player_a = amount;
            g.total_bet_player_a += amount;
            g.pot += amount;
            g.option_player_a = BET;
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
        });

    depositfunds(player, _self, amount, "Player2 made a bet");

    }
    print("End of bet function");
}

void bluff :: accept(uint64_t id, account_name player){

    require_auth(player);
    game_table gt(_self, _self);
    auto itr = gt.find(id);
    eosio_assert(itr != gt.end(), "Game doesnt exist accept");
    eosio_assert(player == itr->turn, "Not your turn accept");
    eosio_assert(itr->game_status == STARTED, "Game has not started yet accept");

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
    nextround(id);
    createcard(id);
}

    
void bluff :: raise(uint64_t id, account_name player, asset amount){

    require_auth(player);
    game_table gt(_self, _self);
    auto itr = gt.find(id);

    eosio_assert(itr != gt.end(), "No such game exists raise");
    eosio_assert(player == itr->turn, "Not your turn raise");
    eosio_assert(itr->game_status == STARTED, "Game has not started yet raise");

    if(player == itr->player_a){

        eosio_assert(amount>itr->curr_bet_player_b, "Amount too low raise");
        eosio_assert(itr->option_player_b != NONE, "Cannot raise now raise");
        eosio_assert(itr->option_player_b != HOLD, "Cannot raise now raise");

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

        eosio_assert(amount > itr->curr_bet_Player_a, "Amount too low raise");
        eosio_assert(itr->option_player_a != NONE, "Cannot raise now raise");
        eosio_assert(itr->option_player_a != HOLD, "Cannot raise now raise");

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
            depositfunds(player, _self, amount, "Player2 raised the bet");
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

void bluff :: hold(uint64_t id, account_name player){

    require_auth(player);
    game_table gt(_self, _self);
    auto itr = gt.find(id);

    eosio_assert(itr != gt.end(), "No such game exist hold");
    eosio_assert(player == itr->turn, "Not your turn hold");
    eosio_assert(itr->game_status == STARTED, "Game has not started yet hold");

    if(player == itr->player_a){

        if(itr->option_player_b == NONE){

            gt.modify(itr, _self,[&](auto &g){
                g.turn = itr->player_b;
                g.option_player_a = HOLD;
            });
            print("Player1 did not place a bet");
        }

        else if(itr->option_player_b == HOLD){
            
            gt.modify(itr, _self,[&](auto &g){
                g.turn = itr->player_b;
                g.option_player_a = HOLD;
            });
            print("Player1 did not place a bet");
            nextround(id);
            createcard(id);
        }

        else{
            
            gt.modify(itr, _self,[&](auto &g){
                g.option_player_a = HOLD;
            });
            fold(id, player);
        }
    }
    else{

        if(itr->option_player_a == NONE){

            gt.modify(itr, _self, [&](auto &g){
                g.turn = itr->player_a;
                g.option_player_b = HOLD;
            });   
            print("Player2 did not place a bet");
        }

        else if(itr->option_player_a == HOLD){
            
            gt.modify(itr, _self,[&](auto &g){
                g.turn = itr->player_a;
                g.option_player_b = HOLD;
            });
            print("Player2 did not place a bet");
            nextround(id);
            createcard(id);
        }

        else{

            gt.modify(itr, _self,[&](auto &g){
                g.option_player_b = HOLD;
            });
            fold(id, player);
        }
    } 
}

void bluff :: revealcards(uint64_t id, account_name player){
    
    char *msg = (char *)malloc(64);
    require_auth(player);
    game_table gt(_self, _self);
    auto itr = gt.find(id);

    if(player == itr->player_a){
        for(auto i = itr->cards_playera.begin(); i != itr->cards_playera.end(); ++i){
            uint8_t card = *i;
            sprintf(msg,"%u",card);
            prints(msg);
        }
    }
    else{
        for(auto i = itr->cards_playerb.begin(); i != itr->cards_playerb.end(); ++i){
            uint8_t card = *i;
            sprintf(msg,"%u",card);
            prints(msg);
        }
    }
}

