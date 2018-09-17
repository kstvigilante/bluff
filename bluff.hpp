#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/symbol.hpp>
#include <eosiolib/crypto.h>

using namespace eosio;
using namespace std;

class bluff: public eosio:: contract{

    public:
        bluff(account_name self): contract(self){};

        //@abi action
        void creategame(account_name player_a); 
        
        //@abi action
        void joingame(uint64_t id, account_name player_b);

        //@abi action
        void fold(uint64_t id, account_name player);

        //@abi action
        void bet(uint64_t id, account_name player ,asset amount);

        //@abi action
        void accept(uint64_t id, account_name player);
    
    private:

        const uint8_t CREATED = 1;
        const uint8_t JOINED = 2;
        const uint8_t STARTED = 3;
        const uint8_t ENDED = 4;

        const uint8_t FOLD = 1;
        const uint8_t HOLD = 2;
        const uint8_t BET = 3;
        const uint8_t RAISE = 4;
        const uint8_t ACCEPT = 5;

        //@abi table games123 i64
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
            uint8_t score_player_a;
            uint8_t score_player_b;
            asset total_bet_player_a;
            asset total_bet_player_b;

            uint64_t primary_key() const { return id; }
            EOSLIB_SERIALIZE(game, (id)(player_a)(player_b)(dealer)(turn)(winner)(pot)(game_status)
                            (option_player_a)(option_player_b)(curr_bet_Player_a)(curr_bet_player_b)
                            (score_player_a)(score_player_b)(total_bet_player_a)(total_bet_player_b))
        };

        typedef eosio::multi_index<N(games123), game> game_table;
        
        void startgame(uint64_t id){
            game_table gt(_self, _self);
            auto itr = gt.find(id);
            eosio_assert(itr != gt.end(), "No such game exists");
            eosio_assert(itr->game_status == JOINED, "Cant begin this game");
            
            gt.modify(itr, _self, [&](auto &g){
                g.game_status = STARTED;
                g.dealer = itr->player_a;
                g.turn = itr->player_b;
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

            action(
            permission_level{ _self, N(active) },
            N(eosio.token), N(transfer),
            make_tuple(_self, winner, amount, string("Congrats you won!!!!")))
            .send();

        }
};

EOSIO_ABI(bluff, (creategame)(joingame)(fold))