#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <eosio/transaction.hpp>
#include <eosio/crypto.hpp>

using namespace eosio;

class [[eosio::contract("defenderfarm")]] defenderfarm : public eosio::contract {

public:

    using contract::contract;

    defenderfarm(name receiver, name code, datastream<const char *> ds) : contract(receiver, code, ds),
                                                                     _events(receiver, receiver.value) {}

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
          //int num = result[i];
          word += arrstr[arrdata[i]];
      }

      create_d(1, "create", word);
      

      //If the sha256 hash generated from data does not equal provided hash, anything below will never fire.
    };

    [[eosio::action]]
    void decrypt(name user, std::string data, checksum256 hash) {
      require_auth(get_self());
      eosio::check(user == get_self(), "Only contract account can do this");
      
      
      assert_sha256((char *)data.c_str(), data.size(), hash);
      print("sha256 hash generated from data equals provided hash");
      print_f("sha256 hash generated from data equals provided hash");
      //create_d(1, "create", here);
      

      //If the sha256 hash generated from data does not equal provided hash, anything below will never fire.
    };

    [[eosio::action]]
    void encrypt(name user, std::string data) {
      require_auth(get_self());
      eosio::check(user == get_self(), "Only contract account can do this");

      checksum256 luna = sha256((char *)data.c_str(), data.size());
      create(1, "create", luna);
      
      print(luna);
    };

    [[eosio::action]]
    void cancel(name user, uint64_t event_id){
      require_auth(get_self());
      eosio::print("Cancelled");
    };

private:
    const int KEY_EXPIRES = 30; // 1800 = 30 min

    struct [[eosio::table]] event {
        int64_t key;
        uint64_t expires;
        uint64_t event_id;
        std::string action;
        checksum256 word;
        std::string decoded;
        uint64_t status;
        uint64_t primary_key() const { return key; }
    };

    typedef eosio::multi_index<name("events"), event> events_table;

    events_table _events;

    void create(
            uint64_t event_id,
            std::string action,
            checksum256 word
    ) {
        _events.emplace(get_self(), [&](auto &new_event) {
            new_event.key = _events.available_primary_key();
            new_event.expires = current_time_point().sec_since_epoch() + KEY_EXPIRES;
            new_event.status = 0;
            new_event.event_id = event_id;
            new_event.action = action;
            new_event.word = word;
        });
    }

    void create_d(
            uint64_t event_id,
            std::string action,
            std::string word
    ) {
        _events.emplace(get_self(), [&](auto &new_event) {
            new_event.key = _events.available_primary_key();
            new_event.expires = current_time_point().sec_since_epoch() + KEY_EXPIRES;
            new_event.status = 0;
            new_event.event_id = event_id;
            new_event.action = action;
            new_event.decoded = word;
        });
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
    if (code == receiver) {
        switch (action) {
            EOSIO_DISPATCH_HELPER(defenderfarm, (encrypt)(cancel)(cleanall)(decrypt)(randomword));
        }
    }
};