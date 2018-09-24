#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/symbol.hpp>
#include <eosiolib/transaction.hpp>
#include <vector>


using namespace eosio;
using namespace std;

class bluff: public eosio:: contract{

    public:
        bluff(account_name self): contract(self){};

        //@abi action
        void creategame(account_name player); 
        
        //@abi action
        void joingame(uint64_t id, account_name player);

        //@abi action
        void fold(uint64_t id, account_name player);

        //@abi action
        void bet(uint64_t id, account_name player ,asset amount);

        //@abi action
        void accept(uint64_t id, account_name player);

        //@abi action
        void raise(uint64_t id, account_name player, asset amount);

        //@abi action
        void hold(uint64_t id, account_name player);

        //@abi action
        void claimwinner(uint64_t id, account_name player);

        
    
    private:

        const uint8_t CREATED = 1;
        const uint8_t JOINED = 2;
        const uint8_t STARTED = 3;
        const uint8_t ENDED = 4;

        const uint8_t NONE = 0;
        const uint8_t FOLD = 1;
        const uint8_t HOLD = 2;
        const uint8_t BET = 3;
        const uint8_t RAISE = 4;
        const uint8_t ACCEPT = 5;

        //@abi table gametc i64
        struct game{
            
            uint64_t id;
            account_name player_a;
            account_name player_b;
            account_name dealer;
            account_name turn;
            account_name winner;
            asset pot;
            uint8_t game_status;
            uint8_t option_player_a;
            uint8_t option_player_b;
            asset curr_bet_Player_a;
            asset curr_bet_player_b;
            asset total_bet_player_a;
            asset total_bet_player_b;
            uint8_t round_number;
            vector<uint8_t> cards_playera;
            vector<uint8_t> cards_playerb;
            time last_playera;
            time last_playerb;

            uint64_t primary_key() const { return id; }
            EOSLIB_SERIALIZE(game, (id)(player_a)(player_b)(dealer)(turn)(winner)(pot)(game_status)
                            (option_player_a)(option_player_b)(curr_bet_Player_a)(curr_bet_player_b)
                            (total_bet_player_a)(total_bet_player_b)
                            (round_number)(cards_playera)(cards_playerb)(last_playera)(last_playerb))            
        };

        //@abi table comii i64
        struct commission{
            uint64_t id;
            asset balance;

            uint64_t primary_key() const{ return id; }
            EOSLIB_SERIALIZE(commission, (id)(balance))
        };

        typedef eosio::multi_index<N(gametc), game> game_table;
        typedef eosio::multi_index<N(comii), commission> comm_table;
        
        void startgame(uint64_t id){

            game_table gt(_self, _self);
            auto itr = gt.find(id);
            eosio_assert(itr != gt.end(), "No such game exists");
            eosio_assert(itr->game_status == JOINED, "Cant begin this game");
            
            gt.modify(itr, _self, [&](auto &g){
                g.game_status = STARTED;
                g.dealer = itr->player_a;
                g.turn = itr->player_b;
                g.round_number = 1;
            });
        }

        void decidewinner(uint64_t id){

            game_table gt(_self, _self);
            auto itr = gt.find(id);
            account_name winner = itr->winner;
            asset amount = itr->pot;

            asset commission;
            asset min_bal = asset(20000, symbol_type(S(4, SYS)));
            if(amount <= min_bal){
                commission = asset(100, symbol_type(S(4, SYS)));
                amount = amount-commission;
            }
            else{
                commission = (amount *5)/100;
                amount = amount-commission;
            }

            depositfunds(_self, winner, amount, "Congratulations you have won the game");

            comm_table ct(_self, _self);
            ct.emplace(_self, [&](auto &c){
                c.id = itr->id;
                c.balance = commission;
            });


        }

        void depositfunds(account_name from, account_name to, asset balance, string memo){

            action(
            permission_level{ from, N(active) },
            N(eosio.token), N(transfer),
            make_tuple(from, to, balance, memo))
            .send();
        }

        void nextround(uint64_t id){

            game_table gt(_self, _self);
            auto itr = gt.find(id);
            eosio_assert(itr != gt.end(), "No such game exists");
            eosio_assert(itr->game_status == STARTED, "Game has not started yet");

            if(itr->round_number == 3){

                revealcards(id);
                uint8_t scoreplayera = 0;
                uint8_t scoreplayerb = 0;

                for(auto i = itr->cards_playera.begin(); i != itr->cards_playera.end(); ++i){
                    scoreplayera += *i;
                }

                for(auto i = itr->cards_playerb.begin(); i != itr->cards_playerb.end(); ++i){
                    scoreplayerb += *i;
                }

                if(scoreplayera == scoreplayerb){

                    asset amount = (itr->pot)/2;
                    account_name playera = itr->player_a;
                    account_name playerb = itr->player_b;
                    depositfunds(_self, playera, amount ,"Game drawn !!!");
                    depositfunds(_self, playerb, amount, "Game drawn !!!");
                    gt.modify(itr, _self, [&](auto &g){
                        g.game_status = ENDED;
                    });

                    comm_table ct(_self, _self);
                    ct.emplace(_self, [&](auto &c){
                        c.id = itr->id;
                        c.balance = asset(00000, symbol_type(S(4, SYS)));
                    });

                }
                else if(scoreplayera > scoreplayerb){
                    
                    gt.modify(itr, _self, [&](auto &g){
                        g.winner = itr->player_a;
                        g.game_status = ENDED;
                    });
                    decidewinner(id);
                }
                else{
                    
                    gt.modify(itr, _self, [&](auto &g){
                        g.winner = itr->player_b;
                        g.game_status = ENDED;
                    });
                    decidewinner(id);
                }
            }
            else{

                gt.modify(itr, _self,[&](auto &g){
                    g.option_player_a = NONE;
                    g.option_player_b = NONE;
                    g.round_number += 1;
                });

                if(itr->dealer == itr->player_a){
                    
                    gt.modify(itr, _self, [&](auto &g){
                        g.turn = itr->dealer;
                        g.dealer = itr->player_b;
                    });
                }
                else{
                    
                    gt.modify(itr, _self, [&](auto &g){
                        g.turn = itr->dealer;
                        g.dealer = itr->player_a;
                    });
                }

            }

        }     

        void createcard(uint64_t id){

            game_table gt(_self, _self);
            auto itr = gt.find(id);

            uint8_t card1 = ((tapos_block_num()+356)*37)%10+1;
            uint8_t card2 = ((tapos_block_num()*234)+82)%10+1;

            gt.modify(itr, _self, [&](auto &g){
                g.cards_playera.push_back(card1);
                g.cards_playerb.push_back(card2);
            });
        }

        void revealcards(uint64_t id){
    
            char *msg = (char *)malloc(64);
            game_table gt(_self, _self);
            auto itr = gt.find(id);

            int i=1;
            auto j = itr->cards_playera.begin();
            print(" Playera cards: ");
            while(i <= 3){
                
                uint8_t card = *j;
                sprintf(msg, " %u",card);
                prints(msg);
                j++;
                i++;
            }

            int k = 1;
            auto l = itr->cards_playerb.begin();
            print(" Playerb cards: ");
            while(k <= 3){

                uint8_t card = *l;
                sprintf(msg, " %u",card);
                prints(msg);
                l++;
                k++;                
            }
        
        }

};

EOSIO_ABI(bluff, (creategame)(joingame)(fold)(bet)(accept)(raise)(hold)(claimwinner))