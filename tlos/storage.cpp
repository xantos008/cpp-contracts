#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <eosio/transaction.hpp>
#include <eosio/crypto.hpp>

using namespace eosio;

class [[eosio::contract("storage")]] storage : public eosio::contract {

public:

    using contract::contract;

    storage(name receiver, name code, datastream<const char *> ds) : contract(receiver, code, ds),
                                                                     _events(receiver, receiver.value) {}

    [[eosio::action]]
    void clean() {
        require_auth(get_self());
        auto _now = current_time_point().sec_since_epoch();

        for (auto itr = _events.begin(); itr != _events.end();) {

            if(itr->status == 4){
                itr = _events.erase(itr);
            } else {

                if (itr->expires <= _now && itr->status == 0) {

                    if(itr->name_owner != get_self()){
                        if(itr->payable){
                            payment(itr->name_owner, itr->price_owner, "Event was expired");
                        }
                    }

                    itr = _events.erase(itr);

                } else {

                    if (itr->accept_expires <= _now && itr->status == 1) {
                        if(itr->payable){
                            payment(itr->name_accept, itr->price_accept, "Accept was expired");
                        }
                        _events.modify(itr, get_self(), [&](auto &item) {
                            item.status = 0;
                        });
                    }

                    itr++;
                }
            }
        }

    }

    [[eosio::action]]
    void cleanall (name user){
        require_auth(get_self());
        eosio::check(user == get_self(), "Only contract account can do this");

        auto itr = _events.begin();

        while(itr != _events.end()){
            itr = _events.erase(itr);
        }
    }

    [[eosio::action]]
    void result(uint64_t event_id, name user, std::string steam_id_winner) {
        require_auth(get_self());
        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        eosio::check(user == get_self(), "Only contract account can do this");
        eosio::check(e.status == 5 || e.status == 3.5 || e.status == 3, "Wrong status. Need 5");

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.status = 6;
            item.steam_id_winner = steam_id_winner;
        });

        std::vector<std::string> users_acc = get_win_account(e, steam_id_winner);
        asset sum;

        for(std::string acc : users_acc){
            int64_t win_user_commission = get_win_account_commission(e, acc);
            if(e.lobby_sponsored){
                if(eventitr->payable){
                    sum = eventitr->price_owner * (100 - win_user_commission) / 100;
                    payment(name(acc), sum, "Won in farmgame. Event id " + std::to_string(event_id));
                }
            } else {
                if(eventitr->payable){
                    sum = eventitr->price_accept * 2 * (100 - win_user_commission) / 100;
                    payment(name(acc), sum, "Won in farmgame. Event id " + std::to_string(event_id));
                }
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
    void changestatus(uint64_t event_id, name user, float status) {
        require_auth(get_self());
        eosio::check(user == get_self(), "Only contract account can do this");

        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        eosio::check(status == 3 || status == 3.5 || status == 4 || status == 5, "Wrong status. Available: 3, 4, 5");

        //if(status == 3) eosio::check(e.status == 2, "Wrong status. Need 2");

        if(status == 4) {
            eosio::check(e.status == 2 || e.status == 3 || e.status == 3.5, "Wrong status. Need 2 or 3");

            if(e.lobby_sponsored && eventitr->name_owner != get_self() && e.payable){
	            payment(eventitr->name_owner, eventitr->price_owner, "Game cancelled");
            }

            if(!e.lobby_sponsored && eventitr->name_owner != get_self() && e.payable){
	            payment(eventitr->name_accept, eventitr->price_accept, "Game cancelled");
	            payment(eventitr->name_owner, eventitr->price_owner, "Game cancelled");
            }

        }

        if(status == 5) {
            eosio::check(e.status == 3 || e.status == 3.5, "Wrong status. Need 3");
            asset diff = eventitr->price_owner - eventitr->price_accept;
            if (diff.amount > 0 && e.payable) payment(eventitr->name_owner, diff, "Pay the difference");
        }
        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.status = status;
            if(status == 3 || status == 3.5) item.lobby_created = current_time_point().sec_since_epoch();
        });
    }

    [[eosio::action]]
    void cancel(uint64_t event_id, name user) {
        require_auth(get_self());
        auto eventitr = _events.find(event_id);

        eosio::check(eventitr != _events.end(), "Cannot find event_id");

        const event &e = *eventitr;

        eosio::check(e.status == 0 || e.status == 1 || e.status == 3 || e.status == 3.5, "Event can't cancelled now");
        eosio::check(e.name_owner == user, "Only event owner can cancel");

        _events.erase(eventitr);

        if(user != get_self() && e.payable){
            if(e.status == 3 || e.status == 3.5){
                payment(eventitr->name_owner, eventitr->price_owner, "Cancelled by owner");
            } else {
               payment(eventitr->name_owner, eventitr->price_owner, "Cancelled by owner");
            }
        }

        if(!e.lobby_sponsored && e.payable){
            if(e.status == 1 || e.status == 3 || e.status == 3.5) payment(eventitr->name_accept, eventitr->price_accept, "Cancelled by owner");
        }

    }

    [[eosio::action]]
    void answer(uint64_t event_id, name user, float status) {
        auto eventitr = _events.find(event_id);

        eosio::check(eventitr != _events.end(), "Cannot find event_id");

        const event &e = *eventitr;

        eosio::check(e.accept_expires > current_time_point().sec_since_epoch(), "The accept is expired");
        eosio::check(e.status == 1, "Event not accepted or already confirmed");
        eosio::check(e.name_owner == user, "Only event owner can confirm");
        eosio::check(status == 0 || status == 2 || status == 3, "Wrong status");

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.status = status;
        });

        if (status == 0 || status == 3) {
            _events.modify(*eventitr, get_self(), [&](auto &item) {
                item.accept_expires = 0;
                item.accepted_at = 0;
                item.rate_total = 0;
                item.player2 = ' ';
                item.steam_id_accept = ' ';
                item.accept_commission = 0;
            });
            if(eventitr->payable){
                payment(eventitr->name_accept, eventitr->price_accept, "Accept was rejected");
            }
        }

        if (status == 2 || status == 3) {
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
        
        if(user_leave_exist && eventitr->lobby_sponsored == false && eventitr->payable){
            payment(user_leave, eventitr->price_accept, "You leaved event");
        }

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.expires = current_time_point().sec_since_epoch() + EVENT_EXPIRES;
            if(e.player1.substr(0, e.player1.find(",")) == user_leave.to_string()) item.player1 = "";
            else if(e.player2.substr(0, e.player2.find(",")) == user_leave.to_string()) item.player2 = "";
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
    void forcedcancel(name user, uint64_t event_id) {
        require_auth(get_self());
        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        eosio::check(user == get_self(), "Only contract account can do this");
        
        if(e.lobby_sponsored){
            if(e.name_owner != get_self()){
                payment(e.name_owner, e.price_owner, "Event was cancelled");
            }
        } else {
            if (e.status > 0 && e.status < 4) {
                 payment(e.name_accept, e.price_accept, "Event was cancelled");
                 payment(e.name_owner, e.price_owner, "Event was cancelled");
            } else {
                if(e.name_owner != get_self()){
                    payment(e.name_owner, e.price_owner, "Event was cancelled");
                }
            }
        }

        _events.erase(eventitr);
    }

    [[eosio::action]]
    void newcontent (
        name user,
        uint64_t event_id,
        std::string player1,
        std::string player2
    ) {
        require_auth(get_self());
        eosio::check(user == get_self(), "Only contract account can do this");
        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.player1 = player1;
            item.player2 = player2;
        });
    }

    [[eosio::action]]
    void screate (name user, uint64_t coast, std::string eventname, std::string server, std::string password, uint64_t starts, uint64_t ends) {
        eosio::check(user == get_self(), "Only contract account can do this");
        eosio::asset new_price(coast, eosio::symbol("TLOS",4));

        uint64_t created = current_time_point().sec_since_epoch();
        uint64_t expires = current_time_point().sec_since_epoch() + 31536000;

        if(password != "0"){
            std::hash<std::string> hash_fn;
            password = std::to_string(hash_fn(password));
        }

        if(starts != 0){
            created = starts;
        }

        if(ends != 0){
            expires = ends;
        }

			uint64_t event_key = _events.available_primary_key();
            _events.emplace(get_self(), [&](auto &new_event) {
                new_event.key = event_key;
                new_event.created_at = created;
                new_event.expires = expires;
                new_event.name_owner = get_self();
                new_event.owner = get_self().to_string() + ",765611982958857132342,0";
                new_event.price_owner = new_price;
                new_event.discipline = "Dota 2";
                new_event.mode = "1 vs 1 Mid";
                new_event.server = server;
                new_event.rate_from = 0;
                new_event.rate_to = coast;
                new_event.rate = coast;
                new_event.player1 = "";
                new_event.steam_id_owner = "7656119829588571323423";
                new_event.lobby_private = password;
                new_event.lobby_sponsored = 1;
                new_event.event_name = eventname;
                new_event.owner_commission = 0;
                new_event.status = 0;
                new_event.payable = 1;

            });
			print(event_key);
    }
	
    [[eosio::action]]
    void anystatus (name user, uint64_t event_id, float status){
        require_auth(get_self());
        eosio::check(user == get_self(), "Only contract account can do this");
        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");

        _events.modify(*eventitr, get_self(), [&](auto &item) {
                item.status = status;
         });
    }

    [[eosio::action]]
    void setsteamser (name user, uint64_t event_id, std::string microsevice){
        require_auth(get_self());
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
    void newlobbyexp(name user, uint64_t event_id) {
        require_auth(get_self());
        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        eosio::check(user == get_self(), "Only contract account can do this");
        
        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.lobby_created = current_time_point().sec_since_epoch();
        });
    }

	[[eosio::action]]
	void schange (name user, uint64_t coast, std::string eventname, std::string server, std::string password, uint64_t event_id, uint64_t starts, uint64_t ends) {
        require_auth(get_self());
        eosio::check(user == get_self(), "Only contract account can do this");
        eosio::asset new_price(coast, eosio::symbol("TLOS",4));
		uint64_t lower_coast = coast - 1000;
        
		auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;
		
		 uint64_t created = current_time_point().sec_since_epoch();
        uint64_t expires = current_time_point().sec_since_epoch() + 31536000;

        if(password != "0"){
            std::hash<std::string> hash_fn;
            password = std::to_string(hash_fn(password));
        }

        if(starts != 0){
            created = starts;
        }

        if(ends != 0){
            expires = ends;
        }
            _events.modify(*eventitr, get_self(), [&](auto &item) {
                item.created_at = created;
                item.expires = expires;
                item.event_name = eventname;
				item.price_owner = new_price;
                item.server = server;
                item.lobby_private = password;
                item.rate = coast;
                item.rate_from = lower_coast;
                item.rate_to = coast;
            });
    }
	
	[[eosio::action]]
    void changeready(name user, uint64_t event_id, uint64_t player_number, bool ready) {
        require_auth(get_self());
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
    void setpayable(name user, uint64_t event_id, std::string data, checksum256 hash) {
        require_auth(get_self());
        eosio::check(user == get_self(), "Only contract account can do this");

        assert_sha256((char *)data.c_str(), data.size(), hash);

        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

       // _events.modify(*eventitr, get_self(), [&](auto &item) {
            //item.payable = 1;
        //});
    }

    [[eosio::action]]
    void randomword(name user, std::string data) {
      require_auth(get_self());
      eosio::check(user == get_self(), "Only contract account can do this");
      std::string str = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
      
      std::vector<std::string> result = explode(data, ',');

      char arrstr[str.length() + 1];
      std::strcpy(arrstr, str.c_str());

      std::string word;      
      
      int n = result.size();
    
      int arrdata[n];
      for (int i = 0; i < n; i++){
          arrdata[i] = std::stoi(result[i]);
      }

      for (int i = 0; i < n; i++) {
          word += arrstr[arrdata[i]];
      }
      
	  print(word);
    };

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

                create(user, owner, quantity, discipline, mode, server, rate_from, rate_to, rate, steam_id, password, lobby_sponsored, event_name, user_commission, true);
            } 
        }
    }

    [[eosio::action]]
    void creteevent(name user, asset quantity, std::string memo) {
        require_auth(get_self());
        if(memo.size() > 25) {
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
            eosio::check(rate_from < rate_to || rate > 0, "rate_from must be more than rate_to or rate most be positive");
            create(user, owner, quantity, discipline, mode, server, rate_from, rate_to, rate, steam_id, password, lobby_sponsored, event_name, user_commission, true);
        }
    }

    [[eosio::action]]
    void moreplayer(name from, asset quantity, std::string memo) {
        require_auth(get_self());
        if (memo.size() > 25) {

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

             accept(event_id, steam_id, from, quantity, user_commission);

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
            eosio::check(quantity.amount > 5000, "must transfer more then 5000");
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

                //create(from, owner, quantity, discipline, mode, server, rate_from, rate_to, rate, steam_id, password, lobby_sponsored, event_name, user_commission, false);
                
            } else {
                //accept(event_id, steam_id, from, quantity, user_commission);
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
        bool payable;
        std::string event_name;
        std::string event_name_steam;
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
        bool player1_ready;
        std::string player2;
        bool player2_ready;
        std::string steam_id_owner;
        std::string steam_id_accept;
        std::string steam_id_winner;
        uint64_t owner_commission;
        uint64_t accept_commission;
        float status;
        std::string microsevice;

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
            uint64_t user_commission,
            bool ispayment
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
            new_event.payable = ispayment;
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
        eosio::check(e.status == 0  || e.status == 3 || e.status == 3.5, "Wrong status");
        eosio::check(quantity.amount >= e.rate_from && quantity.amount <= e.rate_to,
                     "Quantity must be from rate_from to rate_to");

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            std::vector<std::string> players = get_all_players(e);

            int64_t countFreeSpot = 0;

            for (std::string player : players) {
                if(player == "" || player == " "){
                    countFreeSpot++;
                } 
            }

            eosio::check(countFreeSpot >= 1, "No empty slots");

            if(e.player1 == "" || e.player1 == " ") {
                item.player1 = accept.to_string() + "," + steam_id + "," + std::to_string(user_commission);
            } else if(e.player2 == "" || e.player2 == " ") {
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

    //helper functions
    std::vector<std::string> explode(const std::string& str, const char& ch) {
        std::string next;
        std::vector<std::string> result;

        // For each character in the string
        for (std::string::const_iterator it = str.begin(); it != str.end(); it++) {
            // If we've hit the terminal character
            if (*it == ch) {
                // If we have some characters accumulated
                if (!next.empty()) {
                    // Add them to the result vector
                    result.push_back(next);
                    next.clear();
                }
            } else {
                // Accumulate the next character into the sequence
                next += *it;
            }
        }
        if (!next.empty())
            result.push_back(next);
        return result;
    }

    checksum256 encryption(std::string data) {
      checksum256 luna = sha256((char *)data.c_str(), data.size());
      return luna;
    };


};

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
    if (action == name("transfer").value) {
        execute_action<storage>(eosio::name(receiver), eosio::name(code),
                                &storage::transfer_action);
    } else if (code == receiver) {
        switch (action) {
            EOSIO_DISPATCH_HELPER(storage, (creteevent)(moreplayer)(setpayable)(randomword)(newlobbyexp)(setsteamser)(changeready)(anystatus)(screate)(schange)(newcontent)(cleanall)(clean)(forcedcancel)(answer)(neweventname)(leave)(parsponsored)(autocreate)(result)(cancel)(changestatus));
        }
    }
}