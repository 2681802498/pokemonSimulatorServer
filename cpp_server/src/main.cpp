#include <grpcpp/grpcpp.h>
#include "generated/calc.pb.h"
#include "generated/calc.grpc.pb.h"
#include "Battle/BattleSession.h"

#include <nlohmann/json.hpp>

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using namespace calc;
using json = nlohmann::json;

namespace
{

    namespace fs = std::filesystem;

    bool isAllDigits(const std::string &value)
    {
        if (value.empty())
        {
            return false;
        }
        for (char ch : value)
        {
            if (!std::isdigit(static_cast<unsigned char>(ch)))
            {
                return false;
            }
        }
        return true;
    }

    std::string detectDefaultDataDir(const char *argv0)
    {
        std::error_code ec;
        fs::path exePath = fs::absolute(argv0, ec);
        if (!ec)
        {
            fs::path candidate = exePath.parent_path().parent_path() / "data";
            if (fs::exists(candidate, ec) && fs::is_directory(candidate, ec))
            {
                return candidate.string();
            }
        }

        fs::path fallback = fs::current_path(ec) / "data";
        return fallback.string();
    }

    json makeDefaultPokemon(int speciesId, const std::string &abilityName)
    {
        return json{
            {"speciesID", speciesId},
            {"level", 50},
            {"nature", "hardy"},
            {"ability", abilityName},
            {"moves", json::array({1, 2, 5, 8})}};
    }

    json makeDefaultInitRequest()
    {
        return json{
            {"seed", 42},
            {"side_a", json{{"name", "Side A"}, {"pokemon", json::array({makeDefaultPokemon(6, "blaze")})}}},
            {"side_b", json{{"name", "Side B"}, {"pokemon", json::array({makeDefaultPokemon(9, "torrent")})}}}};
    }

} // namespace

class CalculatorServiceImpl final : public Calculator::Service
{
private:
    struct RoomState
    {
        std::mutex mu;
        std::optional<BattleSession> session;
        std::unordered_map<std::string, std::string> playerToSide;
    };

    std::mutex roomsMu_;
    std::unordered_map<std::string, std::shared_ptr<RoomState>> rooms_;
    static constexpr int MAX_CAPACITY = 200;

    static std::string assignSide(RoomState &state, const std::string &playerId)
    {
        auto it = state.playerToSide.find(playerId);
        if (it != state.playerToSide.end())
        {
            return it->second;
        }

        bool sideAUsed = false;
        bool sideBUsed = false;
        for (const auto &entry : state.playerToSide)
        {
            if (entry.second == "a")
            {
                sideAUsed = true;
            }
            else if (entry.second == "b")
            {
                sideBUsed = true;
            }
        }

        const std::string assigned = !sideAUsed ? "a" : (!sideBUsed ? "b" : "a");
        state.playerToSide[playerId] = assigned;
        return assigned;
    }

    static bool parseActionPayload(const std::string &payload, json &result, std::string &err)
    {
        if (payload.empty())
        {
            result = json{{"type", "pass"}};
            return true;
        }

        result = json::parse(payload, nullptr, false);
        if (result.is_discarded())
        {
            err = "action must be valid json";
            return false;
        }
        if (!result.is_object())
        {
            err = "action json must be an object";
            return false;
        }
        return true;
    }

public:
    grpc::Status CreateRoom(grpc::ServerContext *context, const CreateRoomRequest *request, CommonResponse *reply) override
    {
        (void)context;

        std::shared_ptr<RoomState> state = std::make_shared<RoomState>();

        json initRequest;
        if (request->init_json().empty())
        {
            initRequest = makeDefaultInitRequest();
        }
        else
        {
            initRequest = json::parse(request->init_json(), nullptr, false);
            if (initRequest.is_discarded())
            {
                reply->set_code(400);
                reply->set_message("CreateRoom init_json must be valid json");
                return grpc::Status::OK;
            }

            if (initRequest.contains("init") && initRequest["init"].is_object())
            {
                initRequest = initRequest["init"];
            }
        }

        std::string initErr;
        auto session = BattleSession::createFromJson(initRequest, &initErr);
        if (!session.has_value())
        {
            reply->set_code(500);
            reply->set_message("CreateRoom init failed: " + initErr);
            return grpc::Status::OK;
        }

        {
            std::lock_guard<std::mutex> lock(state->mu);
            state->session = std::move(session.value());
        }

        {
            std::lock_guard<std::mutex> lock(roomsMu_);
            rooms_[request->room_id()] = state;
        }

        reply->set_code(0);
        reply->set_message("Room " + request->room_id() + " initialized with BattleSession.");
        return grpc::Status::OK;
    }

    grpc::Status SendCommand(grpc::ServerContext *context, const GameCommand *request, CommonResponse *reply) override
    {
        (void)context;

        std::shared_ptr<RoomState> roomState;
        {
            std::lock_guard<std::mutex> lock(roomsMu_);
            auto it = rooms_.find(request->room_id());
            if (it == rooms_.end())
            {
                reply->set_code(404);
                reply->set_message("Room not found");
                return grpc::Status::OK;
            }
            roomState = it->second;
        }

        std::lock_guard<std::mutex> roomLock(roomState->mu);
        if (!roomState->session.has_value())
        {
            reply->set_code(500);
            reply->set_message("Room session not initialized");
            return grpc::Status::OK;
        }

        json actionJson;
        std::string parseErr;
        if (!parseActionPayload(request->action(), actionJson, parseErr))
        {
            reply->set_code(400);
            reply->set_message(parseErr);
            return grpc::Status::OK;
        }

        const std::string side = assignSide(*roomState, request->player_id());
        if (!actionJson.contains("side"))
        {
            actionJson["side"] = side;
        }
        if (!actionJson.contains("type"))
        {
            actionJson["type"] = "pass";
        }

        json turnRequest;
        if (actionJson.contains("actions") && actionJson["actions"].is_array())
        {
            turnRequest = actionJson;
        }
        else
        {
            turnRequest = json{{"actions", json::array({actionJson})}};
        }

        const json result = roomState->session->processTurn(turnRequest);
        if (!result.value("ok", false))
        {
            reply->set_code(422);
            reply->set_message(result.dump());
            return grpc::Status::OK;
        }

        reply->set_code(0);
        reply->set_message(result.dump());
        return grpc::Status::OK;
    }

    grpc::Status DestroyRoom(grpc::ServerContext *context, const DestroyRoomRequest *request, DestroyRoomResponse *reply) override
    {
        (void)context;

        bool erased = false;
        {
            std::lock_guard<std::mutex> lock(roomsMu_);
            erased = rooms_.erase(request->room_id()) > 0;
        }

        if (erased)
        {
            reply->set_code(0);
            reply->set_message("Room destroyed");
            std::cout << "[Engine] Room " << request->room_id() << " cleaned up." << std::endl;
        }
        else
        {
            reply->set_code(404);
            reply->set_message("Room not found");
        }
        return grpc::Status::OK;
    }

    grpc::Status GetHeartbeat(grpc::ServerContext *context, const HeartbeatRequest *request, HeartbeatResponse *response) override
    {
        {
            (void)context;
            (void)request;
        }

        int totalRooms = 0;
        {
            std::lock_guard<std::mutex> lock(roomsMu_);
            totalRooms = static_cast<int>(rooms_.size());
        }

        response->set_code(0);
        response->set_active_rooms(totalRooms);
        response->set_cpu_usage(0.0f);
        response->set_memory_used(0);
        response->set_max_capacity(MAX_CAPACITY);

        return grpc::Status::OK;
    }
};

void RunServer(std::string port)
{
    std::string server_address("0.0.0.0:" + port);
    CalculatorServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "C++ Worker Engine running on " << server_address << std::endl;
    server->Wait();
}

int main(int argc, char **argv)
{
    std::string port = "50051";
    std::string dataDirArg;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--port" && i + 1 < argc)
        {
            port = argv[++i];
            continue;
        }
        if (arg.rfind("--port=", 0) == 0)
        {
            port = arg.substr(7);
            continue;
        }
        if (arg == "--data-dir" && i + 1 < argc)
        {
            dataDirArg = argv[++i];
            continue;
        }
        if (arg.rfind("--data-dir=", 0) == 0)
        {
            dataDirArg = arg.substr(11);
            continue;
        }

        if (isAllDigits(arg))
        {
            port = arg;
        }
    }

    if (dataDirArg.empty())
    {
        const char *envDataDir = std::getenv("CPP_SERVER_DATA_DIR");
        if (envDataDir != nullptr && std::string(envDataDir).size() > 0)
        {
            dataDirArg = envDataDir;
        }
    }
    if (dataDirArg.empty())
    {
        dataDirArg = detectDefaultDataDir(argv[0]);
    }

    std::error_code ec;
    const fs::path dataDirPath = fs::absolute(dataDirArg, ec);
    const fs::path resolvedDataDir = ec ? fs::path(dataDirArg) : dataDirPath;
    fs::path workingDir = resolvedDataDir.parent_path();
    if (workingDir.empty())
    {
        workingDir = ".";
    }

    std::error_code cwdErr;
    fs::current_path(workingDir, cwdErr);
    if (cwdErr)
    {
        std::cerr << "[Engine] Failed to set working directory to: " << workingDir.string()
                  << ", error=" << cwdErr.message() << std::endl;
    }
    else
    {
        std::cout << "[Engine] Data dir: " << resolvedDataDir.string() << std::endl;
        std::cout << "[Engine] Working dir: " << fs::current_path().string() << std::endl;
    }

    RunServer(port);
    return 0;
}