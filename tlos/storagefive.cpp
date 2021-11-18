#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <eosio/transaction.hpp>
#include <eosio/crypto.hpp>

using namespace eosio;

class [[eosio::contract("storagefive")]] storagefive : public eosio::contract {

public:

    using contract::contract;

    storagefive(name receiver, name code, datastream<const char *> ds) : contract(receiver, code, ds),
                                                                     _events(receiver, receiver.value) {}

    [[eosio::action]]
    void clean() {
        auto _now = current_time_point().sec_since_epoch();

        for (auto itr = _events.begin(); itr != _events.end();) {

            if (itr->expires <= _now && (itr->status == 0 || itr->status == 4) ) {

				cancel_event(itr->key);
                itr = _events.erase(itr);

            } else {
                itr++;
            }
        }

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
    void result(uint64_t event_id, name user, std::string win_side) {

        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        eosio::check(user == get_self(), "Only contract account can do this");
        eosio::check(e.status == 5 || e.status == 3.5 || e.status == 3, "Wrong status. Need 5");

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.status = 6;
            item.side_winner = win_side;
        });

        std::vector<std::string> users_acc = get_win_accounts(e, win_side);
        asset sum;
        for (std::string acc : users_acc) {
            int64_t win_user_commission = get_win_account_commission(e, acc);

            if(e.lobby_sponsored){
                sum = eventitr->price_owner / 10 * 2 * (100 - win_user_commission) / 100;
            } else {
                sum = eventitr->price_owner * 2 * (100 - win_user_commission) / 100;
            }
            if(eventitr->payable){
                payment(name(acc), sum, "Won in farmgame. Event id " + std::to_string(event_id));
            }
        }
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
    void changeside(uint64_t event_id, name user, std::string new_side) {

        auto eventitr = _events.find(event_id);
        //eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        eosio::check(new_side.find("dire") == 0 || new_side.find("radiant") == 0, "Wrong side string. Only dire or radiant available.");
        eosio::check(e.status == 0, "Wrong status. Need 0");
        
        std::vector<std::string> users_acc = get_all_players(e);
        std::string user_side = "";
        std::string user_steamId = "";
        std::string user_str = "";
        std::string user_commission = "";

        for (std::string acc : users_acc) {
            std::string now_user = acc.substr(0, acc.find(","));

            if (user.to_string() == now_user) {
	            int64_t pos1 = acc.find(",");
	            int64_t pos2 = acc.find(",", pos1 + 1);
                int64_t pos3 = acc.find(",", pos2 + 1);
	            int64_t pos4 = acc.size();

                user_steamId = acc.substr(pos1 + 1, pos2 - pos1 - 1);
                user_side = acc.substr(pos2 + 1, pos3 - pos2 - 1);
                user_commission = acc.substr(pos3 + 1);
                user_str = acc;
                break;
            }
        }

        eosio::check(user_side != "", "Player did not participate yet");
        eosio::check(user_side != new_side, "New side equals old side");

		int64_t countEmptySide = 0;

        for (std::string acc : users_acc) {
            int64_t pos1 = acc.find(",");
	        int64_t pos2 = acc.find(",", pos1 + 1);
            int64_t pos3 = acc.find(",", pos2 + 1);
	        int64_t pos4 = acc.size();

			if(acc.substr(pos2 + 1, pos3 - pos2 - 1) == new_side) countEmptySide++;
        }

        eosio::check(countEmptySide < 5, "No empty slots");

		_events.modify(*eventitr, get_self(), [&](auto &item) {
	        if(e.player1 == user_str) item.player1 = user.to_string() + "," + user_steamId + "," + new_side + "," + user_commission;
	        else if(e.player2 == user_str) item.player2 = user.to_string() + "," + user_steamId + "," + new_side + "," + user_commission;
	        else if(e.player3 == user_str) item.player3 = user.to_string() + "," + user_steamId + "," + new_side + "," + user_commission;
	        else if(e.player4 == user_str) item.player4 = user.to_string() + "," + user_steamId + "," + new_side + "," + user_commission;
	        else if(e.player5 == user_str) item.player5 = user.to_string() + "," + user_steamId + "," + new_side + "," + user_commission;
	        else if(e.player6 == user_str) item.player6 = user.to_string() + "," + user_steamId + "," + new_side + "," + user_commission;
	        else if(e.player7 == user_str) item.player7 = user.to_string() + "," + user_steamId + "," + new_side + "," + user_commission;
	        else if(e.player8 == user_str) item.player8 = user.to_string() + "," + user_steamId + "," + new_side + "," + user_commission;
	        else if(e.player9 == user_str) item.player9 = user.to_string() + "," + user_steamId + "," + new_side + "," + user_commission;
	        else if(e.lobby_sponsored && e.player10 == user_str) item.player10 = user.to_string() + "," + user_steamId + "," + new_side + "," + user_commission;
	        else if(!e.lobby_sponsored && e.owner == user_str) item.owner = user.to_string() + "," + user_steamId + "," + new_side + "," + user_commission;
	        else eosio::check(false, user_str + " " + user_side + " user_steamId:" + user_steamId);
	    });
    }

    [[eosio::action]]
    void changestatus(uint64_t event_id, name user, float status) {
        eosio::check(user == get_self(), "Only contract account can do this");

        auto eventitr = _events.find(event_id);
        //eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        eosio::check(status == 3 || status == 3.5 || status == 4 || status == 5, "Wrong status. Available: 3, 4, 5");

        //if(status == 3) eosio::check(e.status == 2, "Wrong status. Need 2");

        if(status == 4) {
            eosio::check(e.status == 2 || e.status == 3 || e.status == 3.5, "Wrong status. Need 2 or 3");

			cancel_event(event_id);
        }

        if(status == 5) eosio::check(e.status == 3 || e.status == 3.5, "Wrong status. Need 3");

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.status = status;
            if(status == 3 || status == 3.5) item.lobby_created = current_time_point().sec_since_epoch();
        });
    }

    [[eosio::action]]
    void startevent(uint64_t event_id, name user) {

        auto eventitr = _events.find(event_id);
        //eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        eosio::check(e.status == 0, "Wrong event status");

		std::string owner = get_owner(e);

        eosio::check(user.to_string() == owner, "Only owner can start event");

		_events.modify(*eventitr, get_self(), [&](auto &item) {
            item.status = 2;
        });
    }
    
    [[eosio::action]]
    void parsponsored(uint64_t event_id, name user, std::string password, std::string steam_id, std::string side, std::string user_commission) {
        eosio::check(side.find("dire") == 0 || side.find("radiant") == 0, "Wrong side string. Only dire or radiant available.");

        auto eventitr = _events.find(event_id);

        //eosio::check(eventitr != _events.end(), "Cannot find event_id");

        const event &e = *eventitr;
        eosio::check(e.status == 0, "Wrong status");

        //eosio::check(e.expires > current_time_point().sec_since_epoch(), "Loo late for participate. Expired.");
        eosio::check(e.lobby_sponsored == true, "It`s sponsored event");
        
        if (e.lobby_private != "0") {
            std::hash<std::string> hash_fn;
            password = std::to_string(hash_fn(password));
            eosio::check(password == e.lobby_private, "Wrong password");
        }
        
        participate_core(event_id, steam_id, user, side, user_commission);
    }

    [[eosio::action]]
	void cancel (uint64_t event_id, name user) {
		auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        eosio::check(e.status == 0, "Wrong status");

		std::string owner = get_owner(e);

        eosio::check(user.to_string() == owner, "Don`t have permission for cancel");
		
		cancel_event(event_id);
		
	}
	
	
    [[eosio::action]]
    void leaveevent(uint64_t event_id, name user, name user_leave) {

        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        eosio::check(e.status == 0, "Wrong status");

		std::string owner = get_owner(e);

        eosio::check(user.to_string() == owner || user == user_leave, "Don`t have permission for kick");

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
			else if(e.player3.substr(0, e.player3.find(",")) == user_leave.to_string()) item.player3 = "";
			else if(e.player4.substr(0, e.player4.find(",")) == user_leave.to_string()) item.player4 = "";
			else if(e.player5.substr(0, e.player5.find(",")) == user_leave.to_string()) item.player5 = "";
			else if(e.player6.substr(0, e.player6.find(",")) == user_leave.to_string()) item.player6 = "";
			else if(e.player7.substr(0, e.player7.find(",")) == user_leave.to_string()) item.player7 = "";
			else if(e.player8.substr(0, e.player8.find(",")) == user_leave.to_string()) item.player8 = "";
			else if(e.player9.substr(0, e.player9.find(",")) == user_leave.to_string()) item.player9 = "";
			else if(e.player10.substr(0, e.player10.find(",")) == user_leave.to_string()) item.player10 = "";
        });

        if (!e.lobby_sponsored && e.payable) payment(user_leave, e.price_owner, "Left event id " + std::to_string(event_id));
    };
	
	[[eosio::action]]
    void forcedcancel(name user, uint64_t event_id) {
        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        eosio::check(user == get_self(), "Only contract account can do this");
        
        cancel_event(event_id);
        _events.erase(eventitr);

        auto _now = current_time_point().sec_since_epoch();

        for (auto itr = _events.begin(); itr != _events.end();) {

            if (itr->expires <= _now && itr->status == 0) {

				cancel_event(itr->key);
                itr = _events.erase(itr);

            } else {                
                itr++;
            }
        }

    }

    [[eosio::action]]
    void newcontent (
        name user,
        uint64_t event_id,
        std::string player1,
        std::string player2,
        std::string player3,
        std::string player4,
        std::string player5,
        std::string player6,
        std::string player7,
        std::string player8,
        std::string player9,
        std::string player10
    ) {
        eosio::check(user == get_self(), "Only contract account can do this");
        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.player1 = player1;
            item.player2 = player2;
            item.player3 = player3;
            item.player4 = player4;
            item.player5 = player5;
            item.player6 = player6;
            item.player7 = player7;
            item.player8 = player8;
            item.player9 = player9;
            item.player10 = player10;
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
                new_event.key = _events.available_primary_key();
                new_event.created_at = created;
                new_event.expires = expires;
                new_event.owner = "farmgamem5v5,7656119829588571323423,dire,0";
                new_event.price_owner = new_price;
                new_event.discipline = "Dota 2";
                new_event.mode = "5 vs 5";
                new_event.server = server;
                new_event.status = 0;
                new_event.lobby_private = password;
                new_event.lobby_sponsored = 1;
                new_event.event_name = eventname;
                new_event.event_name_steam = "";
                new_event.payable = 1;
            });
			print(event_key);
    }
	
	[[eosio::action]]
	void schange (name user, uint64_t coast, std::string eventname, std::string server, std::string password, uint64_t event_id,  uint64_t starts, uint64_t ends) {
        eosio::check(user == get_self(), "Only contract account can do this");
        eosio::asset new_price(coast, eosio::symbol("TLOS",4));
        
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
            if(player_number == 3){
                item.player3_ready = ready;
            }
            if(player_number == 4){
                item.player4_ready = ready;
            }
            if(player_number == 5){
                item.player5_ready = ready;
            }
            if(player_number == 6){
                item.player6_ready = ready;
            }
            if(player_number == 7){
                item.player7_ready = ready;
            }
            if(player_number == 8){
                item.player8_ready = ready;
            }
            if(player_number == 9){
                item.player9_ready = ready;
            }
            if(player_number == 10){
                item.player10_ready = ready;
            }
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
    void newlobbyexp(name user, uint64_t event_id) {

        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        eosio::check(user == get_self(), "Only contract account can do this");
        
        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.lobby_created = current_time_point().sec_since_epoch();
        });
    }

    [[eosio::action]]
    void resbo (name from, name to, uint64_t sum, std::string memo) {
        eosio::check(from == get_self(), "Only contract account can do this");
        eosio::asset new_price(sum, eosio::symbol("TLOS",4));
        //payment(to, new_price, memo);
    };

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
    void setpayable(name user, uint64_t event_id, std::string data, checksum256 hash) {
        require_auth(get_self());
        eosio::check(user == get_self(), "Only contract account can do this");

        assert_sha256((char *)data.c_str(), data.size(), hash);

        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.payable = 1;
        });
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

            std::string discipline = memo.substr(0, pos1);
            std::string mode = memo.substr(pos1 + 1, pos2 - pos1 - 1);
            std::string server = memo.substr(pos2 + 1, pos3 - pos2 - 1);
            std::string password = memo.substr(pos3 + 1, pos4 - pos3 - 1);
            uint64_t price_owner = std::stoll(memo.substr(pos4 + 1, pos5 - pos4 - 1));
            std::string steam_id = memo.substr(pos5 + 1, pos6 - pos5 - 1);
            bool lobby_sponsored = std::stoi(memo.substr(pos6 + 1, pos7 - pos6 - 1));
            uint64_t event_id = std::stoi(memo.substr(pos7 + 1, pos8 - pos7 - 1));
            std::string side = memo.substr(pos8 + 1, pos9 - pos8 - 1);
            std::string event_name = memo.substr(pos9 + 1, pos10 - pos9 - 1);
            std::string user_commission = memo.substr(pos10 + 1, pos11 - pos10 - 1);

            eosio::check(side.find("dire") == 0 || side.find("radiant") == 0, "Wrong side string. Only dire or radiant available.");
            
            if (password != "0") {
                std::hash<std::string> hash_fn;
                password = std::to_string(hash_fn(password));
            }

            if (event_id == -1) {
                eosio::check(price_owner == quantity.amount, "price_owner must be equal quantity.amount");
                create(from, quantity, discipline, mode, server, steam_id, side, password, lobby_sponsored, event_name, user_commission, false);
            } else {
                participate(event_id, steam_id, from, quantity, side, password, user_commission);
            }
        }
    }

private:
    const int EVENT_EXPIRES = 3600; // 3600 = 60 min
    const int COMMISSION = 10;

    struct [[eosio::table]] event {
        int64_t key;
        uint64_t created_at;
        uint64_t expires;
        uint64_t lobby_created;
        std::string lobby_private;
        bool lobby_sponsored;
        bool payable;
        std::string event_name;
        std::string event_name_steam;
        asset price_owner;
        std::string discipline;
        std::string mode;
        std::string server;
        std::string side_winner;
        float status;
        std::string owner; //name,steamid,side
        std::string player1;
        bool player1_ready;
        std::string player2;
        bool player2_ready;
        std::string player3;
        bool player3_ready;
        std::string player4;
        bool player4_ready;
        std::string player5;
        bool player5_ready;
        std::string player6;
        bool player6_ready;
        std::string player7;
        bool player7_ready;
        std::string player8;
        bool player8_ready;
        std::string player9;
        bool player9_ready;
        std::string player10;
        bool player10_ready;
        std::string microsevice;

        uint64_t primary_key() const { return key; }
    };

    typedef eosio::multi_index<name("events"), event> events_table;

    events_table _events;

    void create(
            name owner,
            asset price_owner,
            std::string discipline,
            std::string mode,
            std::string server,
            std::string steam_id,
            std::string side,
            std::string lobby_private,
            bool lobby_sponsored,
            std::string event_name,
            std::string user_commission,
            bool ispayable
    ) {
        uint64_t event_key = _events.available_primary_key();
        _events.emplace(get_self(), [&](auto &new_event) {
            new_event.key = event_key;
            new_event.created_at = current_time_point().sec_since_epoch();
            new_event.expires = current_time_point().sec_since_epoch() + EVENT_EXPIRES;
            new_event.owner = owner.to_string() + "," + steam_id + "," + side + "," + user_commission;
            new_event.price_owner = price_owner;
            new_event.discipline = discipline;
            new_event.mode = mode;
            new_event.server = server;
            new_event.status = 0;
            new_event.lobby_private = lobby_private;
            new_event.lobby_sponsored = lobby_sponsored;
            new_event.event_name = event_name;
            new_event.event_name_steam = "";
            new_event.payable = ispayable;
        });
        print(event_key);
    }

    void participate(uint64_t event_id, std::string steam_id, name user, asset quantity, std::string side, std::string password_hash, std::string user_commission) {
        auto eventitr = _events.find(event_id);

        //eosio::check(eventitr != _events.end(), "Cannot find event_id");

        const event &e = *eventitr;

        //eosio::check(e.expires > current_time_point().sec_since_epoch(), "Too late for participate. Expired.");
        eosio::check(e.lobby_sponsored == false, "It is sponsored event");
        eosio::check(quantity.amount == e.price_owner.amount, "Wrong quantity");
        
        eosio::check(password_hash == e.lobby_private, "Wrong password");

        participate_core(event_id, steam_id, user, side, user_commission);
    }
    
    void participate_core(uint64_t event_id, std::string steam_id, name user, std::string side, std::string user_commission) {
        auto eventitr = _events.find(event_id);
        const event &e = *eventitr;
        
        _events.modify(*eventitr, get_self(), [&](auto &item) {
            std::vector<std::string> players = get_all_players(e);

            int64_t countDire = 0;
            int64_t countFreeSpot = 0;
            int64_t countRadiant = 0;
            
            for (std::string player : players) {
              if (player == "") {
                  countFreeSpot++;
              } else {
                  int64_t pos1 = player.find(",");
                  int64_t pos2 = player.find(",", pos1 + 1);
                  int64_t pos3 = player.find(",", pos2 + 1);
                  int64_t pos4 = player.size();
                  
                  player.substr(pos2 + 1, pos3 - pos2 - 1) == "dire" ? countDire++ : countRadiant++;
              }
            }
            
            eosio::check(countFreeSpot >= 1, "No empty slots");
            
            if (side == "dire") {
                eosio::check(countDire <= 4, "No empty dire slots");
            } else {
                eosio::check(countRadiant <= 4, "No empty radiant slots");
            }
            
            if(e.player1 == "") item.player1 = user.to_string() + "," + steam_id + "," + side + "," + user_commission;
            else if(e.player2 == "") item.player2 = user.to_string() + "," + steam_id + "," + side + "," + user_commission;
            else if(e.player3 == "") item.player3 = user.to_string() + "," + steam_id + "," + side + "," + user_commission;
            else if(e.player4 == "") item.player4 = user.to_string() + "," + steam_id + "," + side + "," + user_commission;
            else if(e.player5 == "") item.player5 = user.to_string() + "," + steam_id + "," + side + "," + user_commission;
            else if(e.player6 == "") item.player6 = user.to_string() + "," + steam_id + "," + side + "," + user_commission;
            else if(e.player7 == "") item.player7 = user.to_string() + "," + steam_id + "," + side + "," + user_commission;
            else if(e.player8 == "") item.player8 = user.to_string() + "," + steam_id + "," + side + "," + user_commission;
            else if(e.player9 == "") item.player9 = user.to_string() + "," + steam_id + "," + side + "," + user_commission;
            else if(e.player10 == "" && e.lobby_sponsored) item.player10 = user.to_string() + "," + steam_id + "," + side + "," + user_commission;
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

    std::vector<std::string> get_all_players(event e) {
        std::vector<std::string> players;

        e.lobby_sponsored ? players.push_back(e.player10) : players.push_back(e.owner);

        players.push_back(e.player1);
        players.push_back(e.player2);
        players.push_back(e.player3);
        players.push_back(e.player4);
        players.push_back(e.player5);
        players.push_back(e.player6);
        players.push_back(e.player7);
        players.push_back(e.player8);
        players.push_back(e.player9);

        return players;
    };

    std::vector<std::string> get_users(event e) {
        std::vector<std::string> players;

        if( e.player1 != "") players.push_back(e.player1.substr(0, e.player1.find(",")));
        else if( e.player2 != "") players.push_back(e.player2.substr(0, e.player2.find(",")));
        else if( e.player3 != "") players.push_back(e.player3.substr(0, e.player3.find(",")));
        else if( e.player4 != "") players.push_back(e.player4.substr(0, e.player4.find(",")));
        else if( e.player5 != "") players.push_back(e.player5.substr(0, e.player5.find(",")));
        else if( e.player6 != "") players.push_back(e.player6.substr(0, e.player6.find(",")));
        else if( e.player7 != "") players.push_back(e.player7.substr(0, e.player7.find(",")));
        else if( e.player8 != "") players.push_back(e.player8.substr(0, e.player8.find(",")));
        else if( e.player9 != "") players.push_back(e.player9.substr(0, e.player9.find(",")));

        if( e.lobby_sponsored && e.player10 != "" ) players.push_back(e.player10.substr(0, e.player10.find(",")));
        else players.push_back(e.owner.substr(0, e.owner.find(",")));

        return players;
    };

    void cancel_event(uint64_t event_id) {
        auto eventitr = _events.find(event_id);
        eosio::check(eventitr != _events.end(), "Cannot find event_id");
        const event &e = *eventitr;

		std::string owner = get_owner(e);

        if(name(owner) != get_self()){

        if (e.lobby_sponsored) {
            if(e.payable){
                payment(name(owner), e.price_owner, "Cancel event id " + std::to_string(event_id));
            }
        } else {
            std::vector<std::string> users_acc = get_users(e);

            for (std::string acc : users_acc) {
                if(e.payable){
                    payment(name(acc), e.price_owner, "Cancel event id " + std::to_string(event_id));
                }
            }
        }

        }

        _events.modify(*eventitr, get_self(), [&](auto &item) {
            item.status = 4;
        });
    }

    std::string get_owner(event e) {
        std::string owner = e.owner;
        owner = owner.substr(0, owner.find(","));

        return owner;
    }

    std::vector<std::string> get_win_accounts(event e, std::string win_side) {
        std::vector<std::string> accounts;

        std::vector<std::string> players = get_all_players(e);

        for (std::string player : players) {
			int64_t pos0 = player.find(",");
			int64_t pos1 = player.find(",", pos0 + 1);
            int64_t pos2 = player.find(",", pos1 + 1);
			int64_t pos3 = player.size();

			std::string player_acc = player.substr(0, pos0);
			std::string player_side = player.substr(pos1 + 1, pos2 - pos1 - 1);
            std::string player_commission = player.substr(pos2 + 1);

			if(player_side == win_side) accounts.push_back(player_acc);
        }

        return accounts;
    };

    int64_t get_win_account_commission(event e, std::string win_account){
        std::vector<std::string> players = get_all_players(e);

        int64_t num = 0;
        std::string player_acc = "";
        std::string player_commission = "";

        for (std::string player : players) {
			int64_t pos0 = player.find(",");
			int64_t pos1 = player.find(",", pos0 + 1);
            int64_t pos2 = player.find(",", pos1 + 1);
			int64_t pos3 = player.size();

			player_acc = player.substr(0, pos0);
            player_commission = player.substr(pos2 + 1);

			if(player_acc == win_account){
                break;
            }
        }

        for (int i = 0; i < player_commission.length(); i++){     
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
        execute_action<storagefive>(eosio::name(receiver), eosio::name(code),
                                &storagefive::transfer_action);
    } else if (code == receiver) {
        switch (action) {
            EOSIO_DISPATCH_HELPER(storagefive, (setpayable)(randomword)(newlobbyexp)(setsteamser)(changeready)(anystatus)(newcontent)(screate)(schange)(startevent)(parsponsored)(leaveevent)(neweventname)(forcedcancel)(clean)(cleanall)(cancel)(changestatus)(changeside)(result)(resbo));
        }
    }
}