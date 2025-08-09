// ai_shell.cpp
// Build: see Makefile below
// Dependencies: readline, libcurl (optional), nlohmann/json (single header)
// This program spawns an external adapter executable to get AI-generated JSON.
// Adapter should read a prompt (stdin or arg) and print JSON to stdout:
// { "command": "...", "explanation": "...", "confirm": true|false }

#include <bits/stdc++.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <regex>
#include <sys/wait.h>
#include <unistd.h>

#include <nlohmann/json.hpp> // nlohmann::json single-header (add to project)

using namespace std;
using json = nlohmann::json;

static const string AI_HDR = "\033[1;36m[AI Suggestion]\033[0m"; // cyan
static const string CMD_HDR = "\033[1;32m[Command]\033[0m";    // green
static const string EXPL_HDR = "\033[1;35m[Explanation]\033[0m"; // magenta
static const string OUT_HDR = "\033[1;37m[Output]\033[0m";     // white
static const string ERR_HDR = "\033[1;31m[Error]\033[0m";     // red
static const string NOTE = "\033[1;34m[Note]\033[0m";         // blue

// Dangerous patterns (case-insensitive)
const vector<string> DANGEROUS_PATTERNS = {
    R"(\brm\b)",
    R"(\brm\s+-rf\b)",
    R"(\bdd\b)",
    R"(\bmkfs\b)",
    R"(\bshutdown\b|\breboot\b|\bpoweroff\b)",
    R"(\b:\s*\(\)\s*\{)", // fork bomb
    R"(curl .* \| .*sh)",
    R"(wget .* \| .*sh)",
    R"(chmod\s+000\s+/)",
    R"(chown\s+.*\s+/)"
};

bool is_potentially_dangerous(const string &cmd) {
    string lower = cmd;
    transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (auto &p : DANGEROUS_PATTERNS) {
        regex re(p, regex::icase);
        if (regex_search(lower, re)) return true;
    }
    return false;
}

// Helper: run adapter (user provides path) with prompt and conversation history.
// Returns raw stdout of adapter.
string run_adapter(const string &adapter_path, const string &prompt,
                   const vector<pair<string,string>> &conversation) {
    int inpipe[2], outpipe[2];
    if (pipe(inpipe) != 0 || pipe(outpipe) != 0) {
        perror("pipe");
        return "";
    }

    pid_t pid = fork();
    if (pid == 0) {
        dup2(inpipe[0], STDIN_FILENO);
        dup2(outpipe[1], STDOUT_FILENO);
        close(inpipe[1]);
        close(outpipe[0]);
        execlp(adapter_path.c_str(), adapter_path.c_str(), (char*)NULL);
        perror("execlp");
        _exit(127);
    } else if (pid > 0) {
        close(inpipe[0]);
        close(outpipe[1]);

        json payload;
        payload["prompt"] = prompt;
        json hist = json::array();
        for (auto &p : conversation) {
            json turn;
            turn["user"] = p.first;
            turn["assistant"] = p.second;
            hist.push_back(turn);
        }
        payload["history"] = hist;

        string payload_str = payload.dump();
        write(inpipe[1], payload_str.c_str(), payload_str.size());
        close(inpipe[1]);

        string out;
        char buf[4096];
        ssize_t r;
        while ((r = read(outpipe[0], buf, sizeof(buf))) > 0) {
            out.append(buf, buf + r);
        }
        close(outpipe[0]);

        int status = 0;
        waitpid(pid, &status, 0);

        return out;
    } else {
        perror("fork");
        return "";
    }
}

string trim(const string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Dynamic prompt: [user@hostname] - [pwd]
string get_dynamic_prompt() {
    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    const char* user = getenv("USER");
    if (!user) user = "unknown";

    char cwd[1024];
    if (!getcwd(cwd, sizeof(cwd))) {
        strcpy(cwd, "?");
    }

    const char* home = getenv("HOME");
    string cwd_str = cwd;
    if (home && cwd_str.find(home) == 0) {
        cwd_str.replace(0, strlen(home), "~");
    }

    ostringstream oss;
    oss << "\033[1;33m[" << user << "@" << hostname << "] - [" << cwd_str << "]\033[0m ";
    return oss.str();
}

int main(int argc, char **argv) {
    string adapter = "./ai_adapter.py";
    if (argc >= 2) adapter = argv[1];

    cout << NOTE << " AI Shell (C++) - adapter: " << adapter << "\033[0m\n";
    cout << NOTE << " Type 'exit' or 'quit' to leave. Use up-arrow for history.\033[0m\n\n";

    vector<pair<string,string>> conversation;

    while (true) {
        char *line = readline(get_dynamic_prompt().c_str());
        if (!line) break;
        string input = trim(string(line));
        free(line);

        if (input.empty()) continue;
        add_history(input.c_str());

        string lower = input;
        transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "exit" || lower == "quit") {
            cout << NOTE << " Bye!\033[0m\n";
            break;
        }

        string raw = run_adapter(adapter, input, conversation);
        if (raw.empty()) {
            cout << ERR_HDR << " Adapter returned empty output or failed.\033[0m\n";
            continue;
        }

        smatch m;
        string json_text;
        regex re(R"(\{[\s\S]*\})");
        if (regex_search(raw, m, re)) {
            json_text = m.str(0);
        } else {
            json_text = raw;
        }

        json ai_out;
        try {
            ai_out = json::parse(json_text);
        } catch (exception &e) {
            cout << ERR_HDR << " Failed to parse adapter JSON: " << e.what() << "\033[0m\n";
            cout << "Raw adapter output:\n" << raw << "\n";
            continue;
        }

        string command = ai_out.value("command", "");
        string explanation = ai_out.value("explanation", "");
        bool confirm_flag = ai_out.value("confirm", false);

        cout << "\n" << AI_HDR << " " << CMD_HDR << "\n";
        cout << "\033[1;32m$ " << command << "\033[0m\n\n";
        cout << EXPL_HDR << "\n" << explanation << "\n\n";
        // If command starts with "cd "
        if (command.compare(0, 3, "cd ") == 0) {
            string dir = command.substr(3);
            dir = trim(dir);
            if (dir.empty()) {
                dir = getenv("HOME") ? getenv("HOME") : "/";
            }
            if (chdir(dir.c_str()) != 0) {
                perror("chdir");
            }
            cout << OUT_HDR << "\n";
            cout << "Changed directory to " << dir << "\n\n";
            // Save context and skip system exec
            conversation.push_back({input, ai_out.dump()});
            continue;
        }


        cout << "\n[Action] Run as-is (r), edit (e), or cancel (c)? ";
        string choice;
        getline(cin, choice);
        transform(choice.begin(), choice.end(), choice.begin(), ::tolower);

        if (choice == "c" || choice == "cancel") {
            cout << NOTE << " Skipping command.\033[0m\n";
            conversation.push_back({input, ai_out.dump()});
            continue;
        } else if (choice == "e" || choice == "edit") {
            cout << NOTE << " Enter new command:\033[0m ";
            string new_cmd;
            getline(cin, new_cmd);
            if (!new_cmd.empty()) {
                command = new_cmd;
            }
        }

        bool dangerous = is_potentially_dangerous(command);
        if (confirm_flag || dangerous) {
            cout << ERR_HDR << " This command looks potentially dangerous.\033[0m\n";
            cout << "Confirm execution? (y/N): ";
            string ans;
            getline(cin, ans);
            transform(ans.begin(), ans.end(), ans.begin(), ::tolower);
            if (ans != "y" && ans != "yes") {
                cout << NOTE << " Skipping command.\033[0m\n";
                conversation.push_back({input, ai_out.dump()});
                continue;
            }
        }

        cout << OUT_HDR << "\n";
        string exec_cmd = "/bin/sh -c '" + command + "'";
        int rc = system(exec_cmd.c_str());
        (void)rc;
        cout << "\n";

        conversation.push_back({input, ai_out.dump()});
        if (conversation.size() > 8) conversation.erase(conversation.begin());
    }

    return 0;
}
