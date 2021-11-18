#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <eosio/transaction.hpp>

using namespace eosio;

class [[eosio::contract("tournament11")]] tournament11 : public eosio::contract {

public:

    using contract::contract;

    tournament11(name receiver, name code, datastream<const char *> ds) : contract(receiver, code, ds),
                                                                     _events(receiver, receiver.value) {}


    [[eosio::action]]
    void cleanall (name user){
        eosio::check(user == get_self(), "Only contract account can do this");

        auto itr = _events.begin();

        while(itr != _events.end()){
            itr = _events.erase(itr);
        }
    }

    [[eosio::action]]
    void result(uint64_t event_id, name user, std::string steam_id_winner) {
        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        eosio::check(user == get_self(), "Only contract account can do this");
        eosio::check(e.status == 5 || e.status == 3.5 || e.status == 3, "Wrong status. Need 5");

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.status = 6;
            item.steam_id_winner = steam_id_winner;
        });

        /*
        if (steam_id_winner == e.steam_id_owner) {
            asset sum = eventitr->price_accept * 2 * (100 - e.owner_commission) / 100;
            payment(e.name_owner, sum, "Won in farmgame");
        } else if (steam_id_winner == e.steam_id_accept) {
            asset sum = eventitr->price_accept * 2 * (100 - e.accept_commission) / 100;
            payment(e.name_accept, sum, "Won in farmgame");
        }
        */
    }

    [[eosio::action]]
    void forcedcancel(name user, uint64_t event_id) {
        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        eosio::check(user == get_self(), "Only contract account can do this");

        _events.erase(eventitr);
    }
	
    [[eosio::action]]
    void anystatus (name user, uint64_t event_id, float status){
        eosio::check(user == get_self(), "Only contract account can do this");
        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");

        _events.modify(*eventitr, get_self(), [&](auto &item) {
                item.status = status;
         });
    }

    [[eosio::action]]
    void setsteamser (name user, uint64_t event_id, std::string microsevice){
        eosio::check(user == get_self(), "Only contract account can do this");
        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;
        eosio::check(e.microsevice.length() < 2, "This event already have lobby");

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.microsevice = microsevice;
            item.lobby_created = current_time_point().sec_since_epoch();
         });
    }

    [[eosio::action]]
    void neweventname(name user, uint64_t event_id, std::string steameventname) {

        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        eosio::check(user == get_self(), "Only contract account can do this");
        
        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.event_name_steam = steameventname;
        });
    }
	
	[[eosio::action]]
    void changeready(name user, uint64_t event_id, uint64_t player_number, bool ready) {

        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        eosio::check(user == get_self(), "Only contract account can do this");
        
        _events.modify(*eventitr, get_self(), [&](auto &item) {
            if(player_number == 1){
                item.player1_ready = ready;
            }
            if(player_number == 2){
                item.player2_ready = ready;
            }
        });
    }

    [[eosio::action]]
    void confirmwin(name user, uint64_t event_id) {

        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        eosio::check(user == get_self(), "Only contract account can do this");

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.status = 6;
            item.win_confirmed = 1;
        });

        if(e.prize_place > 0){
            std::vector<std::string> users_acc = get_win_account(e, e.steam_id_winner);
            asset sum;

            for(std::string acc : users_acc){
                sum = eventitr->price_owner;
                payment(name(acc), sum, "Won in tournnament event. Prize place" + std::to_string(e.prize_place));
            }
        }

    }

    [[eosio::action]]
    void setwinner(uint64_t event_id, name user, std::string steam_id_winner) {
        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        eosio::check(user == get_self(), "Only contract account can do this");

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.status = 6;
            item.steam_id_winner = steam_id_winner;
        });

        if(e.prize_place > 0){
            std::vector<std::string> users_acc = get_win_account(e, steam_id_winner);
            asset sum;

            for(std::string acc : users_acc){
                sum = eventitr->price_owner;
                payment(name(acc), sum, "Won in tournnament event. Prize place" + std::to_string(e.prize_place));
            }
        }
    }

    [[eosio::action]]
    void admincreate(name user, asset quantity, std::string memo, std::string player1, std::string player2, std::string password) {
        if(password == PASSWORD) {
            eosio::check(user == get_self(), "Only contract account can do this");
            eosio::check(memo.size() <= 256, "memo has more than 256 bytes");

            std::string delimiter = ",";

            int64_t pos1 = memo.find(delimiter);
            int64_t pos2 = memo.find(delimiter, pos1 + 1);
            int64_t pos3 = memo.find(delimiter, pos2 + 1);
            int64_t pos4 = memo.find(delimiter, pos3 + 1);
            int64_t pos5 = memo.find(delimiter, pos4 + 1);
            int64_t pos6 = memo.find(delimiter, pos5 + 1);

            std::string discipline = memo.substr(0, pos1);
            std::string mode = memo.substr(pos1 + 1, pos2 - pos1 - 1);
            std::string server = memo.substr(pos2 + 1, pos3 - pos2 - 1);
            uint64_t prize_place = std::stoi(memo.substr(pos3 + 1, pos4 - pos2 - 1));
            std::string event_name = memo.substr(pos4 + 1, pos5 - pos4 - 1);
            uint64_t tournament_id = std::stoi(memo.substr(pos5 + 1, pos6 - pos5 - 1));

            std::string owner = user.to_string() + ",0,0";
            
            create(
                tournament_id,
                user, 
                owner, 
                quantity, 
                discipline, 
                mode, 
                server,
                player1,
                player2,
                prize_place,
                event_name
            ); 
        }
    }

    void transfer_action(name from, name to, asset quantity, std::string memo) {
        if (to == _self) {
            require_auth(from);
            eosio::check(is_account(to), "to account does not exist");
            require_recipient(from);
            require_recipient(to);
            eosio::check(quantity.is_valid(), "invalid quantity");
            eosio::check(quantity.amount > 0, "must transfer positive quantity");
        }
    }

private:

    const int ACCEPT_EXPIRES = 600; // 600 = 10 min
    const int EVENT_EXPIRES = 1800; // 1800 = 30 min
    const int CLEAN_EVERY = 600; // clean expired events
    const std::string PASSWORD = "F7s#4~4ge9=D?s(xuXDhg>KUnPgm$N";
    //const int COMMISSION = 10;

    struct [[eosio::table]] event {
        int64_t key;
        uint64_t created_at;
        uint64_t lobby_created;
        uint64_t tournament_id;
        std::string event_name;
        std::string event_name_steam;
        std::string owner;
        name name_owner;
        asset price_owner;
        std::string discipline;
        std::string mode;
        std::string server;
        std::string player1;
        bool player1_ready;
        std::string player2;
        bool player2_ready;
        std::string steam_id_winner;
        std::string winner;
        uint64_t prize_place;
        bool win_confirmed;
        float status;
        std::string microsevice;

        uint64_t primary_key() const { return key; }
    };

    typedef eosio::multi_index<name("events"), event> events_table;

    events_table _events;

    void create(
            uint64_t tournament_id,
            name name_owner,
            std::string owner,
            asset price_owner,
            std::string discipline,
            std::string mode,
            std::string server,
            std::string player1,
            std::string player2,
            uint64_t prize_place,
            std::string event_name
    ) {
        _events.emplace(get_self(), [&](auto &new_event) {
            new_event.key = _events.available_primary_key();
            new_event.created_at = current_time_point().sec_since_epoch();
            new_event.tournament_id = tournament_id;
            new_event.name_owner = name_owner;
            new_event.owner = owner;
            new_event.price_owner = price_owner;
            new_event.discipline = discipline;
            new_event.mode = mode;
            new_event.server = server;
            new_event.player1 = player1;
            new_event.player2 = player2;
            new_event.status = 0;
            new_event.prize_place = prize_place;
            new_event.event_name = event_name;
        });
    }

    void payment(name to, asset quantity, std::string memo) {
        action(
                permission_level{get_self(), "active"_n},
                "eosio.token"_n,
                "transfer"_n,
                std::make_tuple(get_self(), to, quantity, memo)
        ).send();
    };

    std::vector<std::string> get_all_players(event e){
        std::vector<std::string> players;

        players.push_back(e.player1);
        players.push_back(e.player2);
 
        return players;
    }

    std::vector<std::string> get_win_account (event e, std::string steam_id_winner) {
        std::vector<std::string> accounts;

        std::vector<std::string> players = get_all_players(e);

        for (std::string player : players){
            int64_t pos0 = player.find(",");
            int64_t pos1 = player.find(",", pos0 + 1);
            int64_t pos2 = player.find(",", pos1 + 1);

            std::string player_acc = player.substr(0, pos0);
            std::string player_steam_id = player.substr(pos0 + 1, pos1 - pos0 - 1);
            std::string player_commission = player.substr(pos1 + 1);

            if(player_steam_id == steam_id_winner) accounts.push_back(player_acc);
        }
        return accounts;
    }

};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
    if (action == name("transfer").value) {
        execute_action<tournament11>(eosio::name(receiver), eosio::name(code),
                                &tournament11::transfer_action);
    } else if (code == receiver) {
        switch (action) {
            EOSIO_DISPATCH_HELPER(tournament11, (result)(admincreate)(setwinner)(confirmwin)(changeready)(neweventname)(setsteamser)(anystatus)(forcedcancel)(cleanall));
        }
    }
}