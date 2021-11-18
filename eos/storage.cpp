#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <eosio/transaction.hpp>

using namespace eosio;

class [[eosio::contract("storage")]] storage : public eosio::contract {

public:

    using contract::contract;

    storage(name receiver, name code, datastream<const char *> ds) : contract(receiver, code, ds),
                                                                     _events(receiver, receiver.value) {}

    [[eosio::action]]
    void clean() {
        auto _now = current_time_point().sec_since_epoch();

        bool cando = 1;

        for (auto itr = _events.begin(); itr != _events.end();) {

            if(itr->status == 4){
                itr = _events.erase(itr);
            } else {

                if (itr->expires <= _now && itr->status == 0) {

                    if(itr->name_owner != get_self()){
                        payment(itr->name_owner, itr->price_owner, "Event was expired");
                    }

                    itr = _events.erase(itr);

                } else {

                    if (itr->accept_expires <= _now && itr->status == 1) {
                        payment(itr->name_accept, itr->price_accept, "Accept was expired");
                        _events.modify(itr, get_self(), [&](auto &item) {
                            item.status = 0;
                        });
                    }

                    std::string ev_owner = itr->owner.substr(0, itr->owner.find(','));
                    uint64_t ev_status = itr->status;

                    if(ev_owner == get_self().to_string()){
                        if(ev_status < 4) {
                            cando = 0;
                        }
                    }

                    itr++;
                }
            }
        }

        selfcreationofften(cando);

    }

     void cleanfunction() {
        auto _now = current_time_point().sec_since_epoch();

        bool cando = 1;

        for (auto itr = _events.begin(); itr != _events.end();) {

            if(itr->status == 4){
                itr = _events.erase(itr);
            } else {

                if (itr->expires <= _now && itr->status == 0) {

                    if(itr->name_owner != get_self()){
                        payment(itr->name_owner, itr->price_owner, "Event was expired");
                    }

                    itr = _events.erase(itr);

                } else {

                    if (itr->accept_expires <= _now && itr->status == 1) {
                        payment(itr->name_accept, itr->price_accept, "Accept was expired");
                        _events.modify(itr, get_self(), [&](auto &item) {
                            item.status = 0;
                        });
                    }

                    std::string ev_owner = itr->owner.substr(0, itr->owner.find(','));
                    uint64_t ev_status = itr->status;

                    if(ev_owner == get_self().to_string()){
                        if(ev_status < 4) {
                            cando = 0;
                        }
                    }

                    itr++;
                }
            }
        }

        selfcreationofften(cando);

    }

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
        eosio::check(e.status == 5, "Wrong status. Need 5");

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.status = 6;
            item.steam_id_winner = steam_id_winner;
        });

        std::vector<std::string> users_acc = get_win_account(e, steam_id_winner);
        asset sum;

        for(std::string acc : users_acc){
            int64_t win_user_commission = get_win_account_commission(e, acc);
            if(e.lobby_sponsored){
                sum = eventitr->price_owner * (100 - win_user_commission) / 100;
                payment(name(acc), sum, "Won in farmgame. Event id " + std::to_string(event_id));
                cleanfunction();
            } else {
                sum = eventitr->price_accept * 2 * (100 - win_user_commission) / 100;
                payment(name(acc), sum, "Won in farmgame. Event id " + std::to_string(event_id));
                cleanfunction();
            }
        }

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
    void changestatus(uint64_t event_id, name user, uint64_t status) {
        eosio::check(user == get_self(), "Only contract account can do this");

        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        eosio::check(status == 3 || status == 4 || status == 5, "Wrong status. Available: 3, 4, 5");

        //if(status == 3) eosio::check(e.status == 2, "Wrong status. Need 2");

        if(status == 4) {
            eosio::check(e.status == 2 || e.status == 3, "Wrong status. Need 2 or 3");

            if(e.lobby_sponsored && eventitr->name_owner != get_self()){
	            payment(eventitr->name_accept, eventitr->price_accept, "Game cancelled");
	            payment(eventitr->name_owner, eventitr->price_accept, "Game cancelled");
            }

            cleanfunction();

        }

        if(status == 5) eosio::check(e.status == 3, "Wrong status. Need 3");

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.status = status;
        });
    }

    [[eosio::action]]
    void cancel(uint64_t event_id, name user) {
        auto eventitr = _events.find(event_id);

        eosio::check(eventitr != _events.end(), "Cannot find event_id");

        const event &e = *eventitr;

        eosio::check(e.status == 0 || e.status == 1 || e.status == 3, "Event can't cancelled now");
        eosio::check(e.name_owner == user, "Only event owner can cancel");

        _events.erase(eventitr);

        if(user != get_self()){
            if(e.status == 3){
                payment(eventitr->name_owner, eventitr->price_accept, "Cancelled by owner");
            } else {
                payment(eventitr->name_owner, eventitr->price_owner, "Cancelled by owner");
            }
        }

        if(!e.lobby_sponsored){
            if(e.status == 1 || e.status == 3) payment(eventitr->name_accept, eventitr->price_accept, "Cancelled by owner");
        }

        cleanfunction();

    }

    [[eosio::action]]
    void answer(uint64_t event_id, name user, int64_t status) {
        auto eventitr = _events.find(event_id);

        eosio::check(eventitr != _events.end(), "Cannot find event_id");

        const event &e = *eventitr;

        eosio::check(e.accept_expires > current_time_point().sec_since_epoch(), "The accept is expired");
        eosio::check(e.status == 1, "Event not accepted or already confirmed");
        eosio::check(e.name_owner == user, "Only event owner can confirm");
        eosio::check(status == 0 || status == 2, "Wrong status");

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.status = status;
        });

        if (status == 0) {
            payment(eventitr->name_accept, eventitr->price_accept, "Accept was rejected");
        }

        if (status == 2) {
            asset diff = eventitr->price_owner - eventitr->price_accept;
            if (diff.amount > 0) payment(eventitr->name_owner, diff, "Pay the difference");
            _events.modify(*eventitr, get_self(), [&](auto &item) {
                item.lobby_created = current_time_point().sec_since_epoch();
            });
        }
    }

    [[eosio::action]]
    void parsponsored(uint64_t event_id, name user, std::string password, std::string steam_id, uint64_t user_commission) {

        auto eventitr = _events.find(event_id);

        eosio::check(eventitr != _events.end(), "Cannot find event");

        const event &e = *eventitr;

        eosio::check(e.expires > current_time_point().sec_since_epoch(), "Too late for participate. Expired.");
        eosio::check(e.lobby_sponsored == true, "It's sponsored event");

        if(e.lobby_private != "0"){
            std::hash<std::string> hash_fn;
            password = std::to_string(hash_fn(password));
            eosio::check(password == e.lobby_private, "Wrong password");
        }

        eosio::check(e.steam_id_owner != steam_id, "steam_id must be different");

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            std::vector<std::string> players = get_all_players(e);

            int64_t countFreeSpot = 0;

            for(std::string player : players) {
                if(player == ""){
                    countFreeSpot++;
                }
            }

            eosio::check(countFreeSpot >= 1, "No empty slots");

            if(e.player1 == ""){
                item.player1 = user.to_string() + "," + steam_id + "," + std::to_string(user_commission);
            } else if(e.player2 == "" && e.lobby_sponsored) {
                item.player2 = user.to_string() + "," + steam_id + "," + std::to_string(user_commission);

                item.status = 2;
                item.steam_id_accept = steam_id;
                item.name_accept = user;
                item.price_accept = e.price_owner;
                item.rate_total = e.rate;
                item.accepted_at = current_time_point().sec_since_epoch();
                item.accept_expires = current_time_point().sec_since_epoch() + ACCEPT_EXPIRES;
                item.accept_commission = user_commission;
            }

        });
    }

    [[eosio::action]]
    void leave(uint64_t event_id, name user, name user_leave){
        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        bool user_leave_exist = false;

        std::vector<std::string> players = get_all_players(e);

        for (std::string player : players) {
            if (player != "") {
                int64_t pos2 = player.find(",");

                std::string player_user = player.substr(0, pos2);

                if (player_user == user_leave.to_string()) {
                    user_leave_exist = true;
                    break;
                }
            }
        }

        eosio::check(user_leave_exist, "Cannot find user_leave " + user_leave.to_string());

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            if(e.player1.substr(0, e.player1.find(",")) == user_leave.to_string()) item.player1 = "";
            else if(e.player2.substr(0, e.player2.find(",")) == user_leave.to_string()) item.player2 = "";
        });

    }

    [[eosio::action]]
    void autocreate(name user, asset quantity, std::string memo) {
        if(memo.size() > 25) {
            eosio::check(user == get_self(), "Only contract account can do this");
            eosio::check(memo.size() <= 256, "memo has more than 256 bytes");

            std::string delimiter = ",";

            int64_t pos1 = memo.find(delimiter);
            int64_t pos2 = memo.find(delimiter, pos1 + 1);
            int64_t pos3 = memo.find(delimiter, pos2 + 1);
            int64_t pos4 = memo.find(delimiter, pos3 + 1);
            int64_t pos5 = memo.find(delimiter, pos4 + 1);
            int64_t pos6 = memo.find(delimiter, pos5 + 1);
            int64_t pos7 = memo.find(delimiter, pos6 + 1);
            int64_t pos8 = memo.find(delimiter, pos7 + 1);
            int64_t pos9 = memo.find(delimiter, pos8 + 1);
            int64_t pos10 = memo.find(delimiter, pos9 + 1);
            int64_t pos11 = memo.find(delimiter, pos10 + 1);
            int64_t pos12 = memo.find(delimiter, pos11 + 1);

            std::string discipline = memo.substr(0, pos1);
            std::string mode = memo.substr(pos1 + 1, pos2 - pos1 - 1);
            std::string server = memo.substr(pos2 + 1, pos3 - pos2 - 1);
            int64_t rate_from = std::stoll(memo.substr(pos3 + 1, pos4 - pos3 - 1));
            int64_t rate_to = std::stoll(memo.substr(pos4 + 1, pos5 - pos4 - 1));
            std::string steam_id = memo.substr(pos5 + 1, pos6 - pos5 - 1);
            uint64_t event_id = std::stoi(memo.substr(pos6 + 1, pos7 - pos6 - 1));

            std::string event_name = memo.substr(pos7 + 1, pos8 - pos7 - 1);
            bool lobby_sponsored = std::stoi(memo.substr(pos8 + 1, pos9 - pos8 - 1));
            std::string password = memo.substr(pos9 + 1, pos10 - pos9 - 1);
            uint64_t rate = std::stoll(memo.substr(pos10 + 1, pos11 - pos10 - 1));
            int64_t user_commission = std::stoi(memo.substr(pos11 + 1, pos12 - pos11 - 1));

            std::string owner = user.to_string() + "," + steam_id + "," + std::to_string(user_commission);

            if(password != "0"){
                std::hash<std::string> hash_fn;
                password = std::to_string(hash_fn(password));
            }

            if (event_id == -1) {
                eosio::check(rate_from < rate_to || rate > 0, "rate_from must be more than rate_to or rate most be positive");

                create(user, owner, quantity, discipline, mode, server, rate_from, rate_to, rate, steam_id, password, lobby_sponsored, event_name, user_commission);
            } 
        }
    }

    void transfer_action(name from, name to, asset quantity, std::string memo) {
        if (to == _self && memo.size() > 25) {
            require_auth(from);
            eosio::check(is_account(to), "to account does not exist");
            require_recipient(from);
            require_recipient(to);
            eosio::check(quantity.is_valid(), "invalid quantity");
            eosio::check(quantity.amount > 0, "must transfer positive quantity");
            eosio::check(memo.size() <= 256, "memo has more than 256 bytes");

            std::string delimiter = ",";

            int64_t pos1 = memo.find(delimiter);
            int64_t pos2 = memo.find(delimiter, pos1 + 1);
            int64_t pos3 = memo.find(delimiter, pos2 + 1);
            int64_t pos4 = memo.find(delimiter, pos3 + 1);
            int64_t pos5 = memo.find(delimiter, pos4 + 1);
            int64_t pos6 = memo.find(delimiter, pos5 + 1);
            int64_t pos7 = memo.find(delimiter, pos6 + 1);
            int64_t pos8 = memo.find(delimiter, pos7 + 1);
            int64_t pos9 = memo.find(delimiter, pos8 + 1);
            int64_t pos10 = memo.find(delimiter, pos9 + 1);
            int64_t pos11 = memo.find(delimiter, pos10 + 1);
            int64_t pos12 = memo.find(delimiter, pos11 + 1);

            std::string discipline = memo.substr(0, pos1);
            std::string mode = memo.substr(pos1 + 1, pos2 - pos1 - 1);
            std::string server = memo.substr(pos2 + 1, pos3 - pos2 - 1);
            int64_t rate_from = std::stoll(memo.substr(pos3 + 1, pos4 - pos3 - 1));
            int64_t rate_to = std::stoll(memo.substr(pos4 + 1, pos5 - pos4 - 1));
            std::string steam_id = memo.substr(pos5 + 1, pos6 - pos5 - 1);
            uint64_t event_id = std::stoi(memo.substr(pos6 + 1, pos7 - pos6 - 1));

            std::string event_name = memo.substr(pos7 + 1, pos8 - pos7 - 1);
            bool lobby_sponsored = std::stoi(memo.substr(pos8 + 1, pos9 - pos8 - 1));
            std::string password = memo.substr(pos9 + 1, pos10 - pos9 - 1);
            uint64_t rate = std::stoll(memo.substr(pos10 + 1, pos11 - pos10 - 1));
            int64_t user_commission = std::stoi(memo.substr(pos11 + 1, pos12 - pos11 - 1));

            std::string owner = from.to_string() + "," + steam_id + "," + std::to_string(user_commission);

            if(password != "0"){
                std::hash<std::string> hash_fn;
                password = std::to_string(hash_fn(password));
            }

            if (event_id == -1) {
                eosio::check(rate_from < rate_to || rate > 0, "rate_from must be more than rate_to or rate most be positive");
                eosio::check(rate_to == quantity.amount || rate == quantity.amount, "rate_to or rate must be equal quantity.amount");

                create(from, owner, quantity, discipline, mode, server, rate_from, rate_to, rate, steam_id, password, lobby_sponsored, event_name, user_commission);
            } else {
                accept(event_id, steam_id, from, quantity, user_commission);
            }
        }
    }

private:

    const int ACCEPT_EXPIRES = 600; // 600 = 10 min
    const int EVENT_EXPIRES = 1800; // 1800 = 30 min
    const int CLEAN_EVERY = 600; // clean expired events
    //const int COMMISSION = 10;

    struct [[eosio::table]] event {
        int64_t key;
        uint64_t expires;
        uint64_t accept_expires;
        uint64_t created_at;
        uint64_t accepted_at;
        uint64_t lobby_created;
        std::string lobby_private;
        bool lobby_sponsored;
        std::string event_name;
        std::string owner;
        name name_owner;
        name name_accept;
        asset price_owner;
        asset price_accept;
        std::string discipline;
        std::string mode;
        std::string server;
        int64_t rate;
        int64_t rate_from;
        int64_t rate_to;
        int64_t rate_total;
        std::string player1;
        std::string player2;
        std::string steam_id_owner;
        std::string steam_id_accept;
        std::string steam_id_winner;
        uint64_t owner_commission;
        uint64_t accept_commission;
        uint64_t status;

        uint64_t primary_key() const { return key; }
    };

    typedef eosio::multi_index<name("events"), event> events_table;

    events_table _events;

    void create(
            name name_owner,
            std::string owner,
            asset price_owner,
            std::string discipline,
            std::string mode,
            std::string server,
            int64_t rate_from,
            int64_t rate_to,
            int64_t rate,
            std::string steam_id,
            std::string lobby_private,
            bool lobby_sponsored,
            std::string event_name,
            uint64_t user_commission
    ) {
        _events.emplace(get_self(), [&](auto &new_event) {
            new_event.key = _events.available_primary_key();
            new_event.created_at = current_time_point().sec_since_epoch();
            new_event.expires = current_time_point().sec_since_epoch() + EVENT_EXPIRES;
            new_event.name_owner = name_owner;
            new_event.owner = owner;
            new_event.price_owner = price_owner;
            new_event.discipline = discipline;
            new_event.mode = mode;
            new_event.server = server;
            new_event.rate_from = rate_from;
            new_event.rate_to = rate_to;
            new_event.rate = rate;
            new_event.player1 = lobby_sponsored ? "" : owner;
            new_event.steam_id_owner = steam_id;
            new_event.lobby_private = lobby_private;
            new_event.lobby_sponsored = lobby_sponsored;
            new_event.event_name = event_name;
            new_event.owner_commission = user_commission;
            new_event.status = 0;
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

    void accept(uint64_t event_id, std::string steam_id, name accept, asset quantity, uint64_t user_commission) {
        auto eventitr = _events.find(event_id);

        eosio::check(eventitr != _events.end(), "Cannot find event_id");

        const event &e = *eventitr;

        eosio::check(e.expires > current_time_point().sec_since_epoch(), "The event is expired");
        eosio::check(e.status == 0, "Wrong status");
        eosio::check(e.steam_id_owner != steam_id, "steam_id must be different");
        eosio::check(quantity.amount >= e.rate_from && quantity.amount <= e.rate_to,
                     "Quantity must be from rate_from to rate_to");

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            std::vector<std::string> players = get_all_players(e);

            int64_t countFreeSpot = 0;

            for (std::string player : players) {
                if(player == ""){
                    countFreeSpot++;
                } 
            }

            eosio::check(countFreeSpot >= 1, "No empty slots");

            if(e.player1 == "") {
                item.player1 = accept.to_string() + "," + steam_id + "," + std::to_string(user_commission);
            } else if(e.player2 == "") {
                item.player2 = accept.to_string() + "," + steam_id + "," + std::to_string(user_commission);

                item.status = 1;
                item.steam_id_accept = steam_id;
                item.name_accept = accept;
                item.price_accept = quantity;
                item.rate_total = quantity.amount;
                item.accepted_at = current_time_point().sec_since_epoch();
                item.accept_expires = current_time_point().sec_since_epoch() + ACCEPT_EXPIRES;
                item.accept_commission = user_commission;

            }
        });
    }

    void selfcreationofften (bool cando) {

        //eosio::check(cando, "Contract event already started");
        if(cando){
        eosio::asset new_price(10000, eosio::symbol("EOS",4));


            _events.emplace(get_self(), [&](auto &new_event) {
                new_event.key = _events.available_primary_key();
                new_event.created_at = current_time_point().sec_since_epoch();
                new_event.expires = current_time_point().sec_since_epoch() + 360000000000;
                new_event.name_owner = get_self();
                new_event.owner = get_self().to_string() + ",765611982958857132342,0";
                new_event.price_owner = new_price;
                new_event.discipline = "Dota 2";
                new_event.mode = "1 vs 1 Mid";
                new_event.server = "3";
                new_event.rate_from = 0;
                new_event.rate_to = 10000;
                new_event.rate = 10000;
                new_event.player1 = "";
                new_event.steam_id_owner = "7656119829588571323423";
                new_event.lobby_private = "0";
                new_event.lobby_sponsored = 1;
                new_event.event_name = "Farmgame 1vs1";
                new_event.owner_commission = 0;
                new_event.status = 0;

            });
        }
    }

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

    int64_t get_win_account_commission(event e, std::string steam_id_winner) {
        std::vector<std::string> players = get_all_players(e);

        int64_t num = 0;
        std::string player_acc = "";
        std::string player_steam_id = "";
        std::string player_commission = "";

        for (std::string player : players) {
            int64_t pos0 = player.find(",");
            int64_t pos1 = player.find(",", pos0 + 1);
            int64_t pos2 = player.find(",", pos1 + 1);

            player_acc = player.substr(0, pos0);
            player_steam_id = player.substr(pos0 + 1, pos1 - pos0 - 1);
            player_commission = player.substr(pos1 + 1);

            if(player_steam_id == steam_id_winner){
                break;
            }
        }

        for(int i = 0; i < player_commission.length(); i++){
            num = num*10 + player_commission[i] - 0x30;
        }

        return num;

    }


};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
    if (action == name("transfer").value) {
        execute_action<storage>(eosio::name(receiver), eosio::name(code),
                                &storage::transfer_action);
    } else if (code == receiver) {
        switch (action) {
            EOSIO_DISPATCH_HELPER(storage, (cleanall)(clean)(answer)(leave)(parsponsored)(autocreate)(result)(cancel)(changestatus));
        }
    }
}