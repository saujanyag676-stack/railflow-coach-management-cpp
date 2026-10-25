#ifdef _WIN32
#define _WIN32_WINNT 0x0601
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SOCKET = int;
constexpr int INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;
constexpr int SD_BOTH = SHUT_RDWR;
static int closesocket(int socket) { return close(socket); }
#endif

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// Train Coach Management System
// One-file prototype: custom doubly linked list + HTTP API + embedded SaaS UI.
// Windows: g++ -std=c++17 -O2 main.cpp -o railflow.exe -lws2_32
// Linux:   g++ -std=c++17 -O2 main.cpp -o railflow

struct Coach {
    int id;
    std::string code;
    std::string type;
    int capacity;
};

struct CoachNode {
    Coach data;
    CoachNode* prev;
    CoachNode* next;
    explicit CoachNode(Coach value) : data(std::move(value)), prev(nullptr), next(nullptr) {}
};

class CoachList {
    CoachNode* head_ = nullptr;
    CoachNode* tail_ = nullptr;
    int nextId_ = 1;
    std::size_t size_ = 0;

public:
    ~CoachList() { clear(); }
    CoachList(const CoachList&) = delete;
    CoachList& operator=(const CoachList&) = delete;
    CoachList() = default;

    int pushBack(const std::string& code, const std::string& type, int capacity) {
        CoachNode* node = new CoachNode({nextId_++, code, type, capacity});
        node->prev = tail_;
        if (tail_) tail_->next = node;
        else head_ = node;
        tail_ = node;
        ++size_;
        return node->data.id;
    }

    int pushFront(const std::string& code, const std::string& type, int capacity) {
        CoachNode* node = new CoachNode({nextId_++, code, type, capacity});
        node->next = head_;
        if (head_) head_->prev = node;
        else tail_ = node;
        head_ = node;
        ++size_;
        return node->data.id;
    }

    int insertAfter(int afterId, const std::string& code, const std::string& type, int capacity) {
        CoachNode* current = find(afterId);
        if (!current) return -1;
        if (current == tail_) return pushBack(code, type, capacity);

        CoachNode* node = new CoachNode({nextId_++, code, type, capacity});
        node->prev = current;
        node->next = current->next;
        current->next->prev = node;
        current->next = node;
        ++size_;
        return node->data.id;
    }

    bool remove(int id) {
        CoachNode* node = find(id);
        if (!node) return false;
        if (node->prev) node->prev->next = node->next;
        else head_ = node->next;
        if (node->next) node->next->prev = node->prev;
        else tail_ = node->prev;
        delete node;
        --size_;
        return true;
    }

    void clear() {
        CoachNode* current = head_;
        while (current) {
            CoachNode* next = current->next;
            delete current;
            current = next;
        }
        head_ = tail_ = nullptr;
        size_ = 0;
    }

    void seed() {
        clear();
        nextId_ = 1;
        pushBack("ENG-01", "Locomotive", 2);
        pushBack("A1-204", "First Class AC", 48);
        pushBack("B2-118", "Sleeper", 72);
        pushBack("D1-076", "Dining", 36);
        pushBack("LUG-09", "Luggage", 12);
    }

    std::vector<Coach> traverse(bool reverse = false) const {
        std::vector<Coach> result;
        result.reserve(size_);
        if (!reverse) {
            for (CoachNode* n = head_; n; n = n->next) result.push_back(n->data);
        } else {
            for (CoachNode* n = tail_; n; n = n->prev) result.push_back(n->data);
        }
        return result;
    }

    std::size_t size() const { return size_; }

private:
    CoachNode* find(int id) const {
        for (CoachNode* n = head_; n; n = n->next) {
            if (n->data.id == id) return n;
        }
        return nullptr;
    }
};

static CoachList gCoaches;
static std::mutex gMutex;

static std::string jsonEscape(const std::string& value) {
    std::ostringstream out;
    for (char c : value) {
        switch (c) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: out << c;
        }
    }
    return out.str();
}

static std::string jsonString(const std::string& body, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    std::size_t p = body.find(token);
    if (p == std::string::npos) return "";
    p = body.find(':', p + token.size());
    if (p == std::string::npos) return "";
    p = body.find('"', p + 1);
    if (p == std::string::npos) return "";
    std::string result;
    for (++p; p < body.size(); ++p) {
        if (body[p] == '"') break;
        if (body[p] == '\\' && p + 1 < body.size()) {
            ++p;
            if (body[p] == 'n') result += '\n';
            else result += body[p];
        } else result += body[p];
    }
    return result;
}

static int jsonInt(const std::string& body, const std::string& key, int fallback = 0) {
    const std::string token = "\"" + key + "\"";
    std::size_t p = body.find(token);
    if (p == std::string::npos) return fallback;
    p = body.find(':', p + token.size());
    if (p == std::string::npos) return fallback;
    ++p;
    while (p < body.size() && std::isspace(static_cast<unsigned char>(body[p]))) ++p;
    char* end = nullptr;
    long value = std::strtol(body.c_str() + p, &end, 10);
    return end == body.c_str() + p ? fallback : static_cast<int>(value);
}

static std::string coachJson(bool reverse) {
    std::lock_guard<std::mutex> lock(gMutex);
    const auto coaches = gCoaches.traverse(reverse);
    int capacity = 0;
    for (const auto& c : coaches) capacity += c.capacity;
    std::ostringstream out;
    out << "{\"count\":" << coaches.size() << ",\"capacity\":" << capacity << ",\"coaches\":[";
    for (std::size_t i = 0; i < coaches.size(); ++i) {
        if (i) out << ',';
        const Coach& c = coaches[i];
        out << "{\"id\":" << c.id << ",\"code\":\"" << jsonEscape(c.code)
            << "\",\"type\":\"" << jsonEscape(c.type) << "\",\"capacity\":" << c.capacity << '}';
    }
    out << "]}";
    return out.str();
}

static const char* kPage = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>RailFlow — Coach Operations</title>
<style>
:root{--ink:#162119;--muted:#67716b;--paper:#f4f5ef;--card:#fff;--line:#dfe4dc;--green:#153f2f;--lime:#c9f06b;--danger:#c84a42;--shadow:0 18px 50px rgba(28,49,38,.08)}
*{box-sizing:border-box}html,body{height:100%;overflow:hidden}body{margin:0;background:var(--paper);color:var(--ink);font-family:Inter,"Segoe UI",Arial,sans-serif}button,input,select{font:inherit}
.shell{height:100vh;display:grid;grid-template-columns:230px 1fr;overflow:hidden}.sidebar{background:#102d22;color:#fff;padding:24px 20px;display:flex;flex-direction:column;overflow:hidden}.brand{display:flex;align-items:center;gap:11px;font-weight:800;font-size:20px}.brandmark{width:35px;height:35px;border-radius:11px;background:var(--lime);display:grid;place-items:center;color:#153f2f}.brandmark svg{width:19px}.nav{margin-top:34px;display:grid;gap:8px}.nav a{color:#b9c9c0;text-decoration:none;padding:11px 13px;border-radius:10px;display:flex;gap:11px;align-items:center;font-size:14px}.nav a.active{background:#214b3b;color:white}.nav svg{width:18px}.status{margin-top:auto;padding:13px;border:1px solid #315547;border-radius:13px;color:#c1d0c8;font-size:12px}.status b{display:block;color:white;margin-bottom:5px}.dot{display:inline-block;width:8px;height:8px;background:#87d764;border-radius:50%;margin-right:6px}
.main{height:100vh;padding:22px 32px;overflow:hidden;display:grid;grid-template-rows:auto auto minmax(0,1fr)}.top{display:flex;align-items:center;justify-content:space-between}.eyebrow{font-size:11px;letter-spacing:.12em;text-transform:uppercase;color:#738078;font-weight:700}.title{font-size:27px;margin:4px 0 0;letter-spacing:-.03em}.avatar{width:38px;height:38px;border-radius:50%;background:#dbe5dc;display:grid;place-items:center;font-weight:700;color:#315342}.stats{display:grid;grid-template-columns:repeat(3,1fr);gap:14px;margin:17px 0}.stat{background:var(--card);border:1px solid var(--line);border-radius:15px;padding:15px 18px;box-shadow:var(--shadow)}.stathead{color:var(--muted);font-size:12px;display:flex;justify-content:space-between}.stat strong{display:block;font-size:24px;margin-top:6px;letter-spacing:-.04em}.stat small{color:#708078;font-size:12px}.grid{min-height:0;display:grid;grid-template-columns:minmax(0,1fr) 330px;gap:16px}.panel{min-height:0;background:var(--card);border:1px solid var(--line);border-radius:17px;box-shadow:var(--shadow);overflow:hidden;display:flex;flex-direction:column}.panelhead{flex:0 0 auto;padding:15px 19px;border-bottom:1px solid var(--line);display:flex;align-items:center;justify-content:space-between}.panelhead h2{font-size:16px;margin:0}.seg{padding:4px;background:#eef1eb;border-radius:9px;display:flex}.seg button{border:0;background:transparent;color:#647069;padding:6px 9px;border-radius:7px;font-size:12px;cursor:pointer}.seg button.active{background:white;color:#163c2d;box-shadow:0 2px 7px #bcc6bd}.route{flex:1;min-height:0;padding:26px 18px;overflow:hidden;display:flex;align-items:flex-start;justify-content:center}.track{width:100%;display:flex;align-items:stretch;justify-content:center;gap:0}.coach{flex:1;min-width:112px;max-width:150px;position:relative}.coachbox{margin:0 8px;background:#f7f8f4;border:1px solid #d9ded6;border-radius:14px;padding:14px 12px 11px;min-height:132px;position:relative;transition:.18s}.coachbox:hover{transform:translateY(-3px);border-color:#9aab9d;box-shadow:0 11px 23px rgba(24,55,40,.1)}.coach:not(:last-child):after{content:"";position:absolute;right:-2px;top:63px;width:14px;border-top:3px solid #748479}.coach:not(:last-child):before{content:"";position:absolute;right:3px;top:58px;width:7px;height:10px;border:2px solid #748479;border-radius:2px;z-index:2;background:white}.coachcode{font-weight:800;font-size:15px}.coachtype{font-size:12px;color:var(--muted);height:28px;margin-top:5px}.cap{display:flex;align-items:center;gap:5px;font-size:11px;color:#355242;margin-top:9px}.remove{position:absolute;right:8px;top:8px;border:0;background:transparent;color:#aab2ad;cursor:pointer;font-size:17px;line-height:1}.remove:hover{color:var(--danger)}.wheels{display:flex;justify-content:space-between;margin:8px 9px 0}.wheels i{width:10px;height:10px;border-radius:50%;background:#536159;box-shadow:22px 0 #536159}.empty{text-align:center;color:var(--muted);padding:70px 20px}.form{padding:15px 18px}.field{margin-bottom:10px}.field label{display:block;font-size:11px;font-weight:700;margin-bottom:5px;color:#46554d}.field input,.field select{width:100%;border:1px solid #d8ded7;border-radius:10px;padding:9px 11px;background:#fbfcf9;outline:none}.field input:focus,.field select:focus{border-color:#4f8068;box-shadow:0 0 0 3px #dce9df}.btn{width:100%;border:0;border-radius:11px;background:var(--green);color:white;padding:10px;font-weight:700;cursor:pointer}.btn:hover{background:#1b503c}.helper{margin-top:8px;color:#7b877f;font-size:10px;line-height:1.35}.toast{position:fixed;right:26px;bottom:25px;background:#173d2e;color:white;padding:13px 17px;border-radius:11px;box-shadow:0 15px 35px #16332640;opacity:0;transform:translateY(12px);pointer-events:none;transition:.2s}.toast.show{opacity:1;transform:none}.actions{display:flex;gap:8px}.ghost{border:1px solid #d8ded7;background:white;border-radius:9px;padding:6px 9px;font-size:12px;cursor:pointer;color:#526158}
.grid.view{display:none}.grid.view.active{display:grid}.modulebody{padding:16px 18px;min-height:0;overflow:hidden}.registry{width:100%;border-collapse:collapse;font-size:12px}.registry th{text-align:left;color:#718078;font-size:10px;text-transform:uppercase;letter-spacing:.08em;padding:0 10px 9px;border-bottom:1px solid var(--line)}.registry td{padding:11px 10px;border-bottom:1px solid #edf0eb}.registry tr:last-child td{border-bottom:0}.codepill{display:inline-block;padding:4px 8px;border-radius:7px;background:#eef3ec;color:#214b3b;font-weight:800}.badge{display:inline-flex;align-items:center;gap:5px;color:#397052;font-size:11px}.badge:before{content:"";width:6px;height:6px;border-radius:50%;background:#62ad7c}.rowremove{border:0;background:#f9eeee;color:#a54742;border-radius:7px;padding:5px 8px;cursor:pointer}.summarylist{display:grid;gap:10px}.summaryitem{background:#f7f8f4;border:1px solid var(--line);border-radius:11px;padding:12px}.summaryitem span{display:block;color:var(--muted);font-size:11px}.summaryitem strong{display:block;margin-top:4px;font-size:19px}.chart{display:grid;gap:13px}.barrow{display:grid;grid-template-columns:110px 1fr 40px;align-items:center;gap:10px;font-size:11px}.barlabel{white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.bartrack{height:10px;background:#edf1eb;border-radius:99px;overflow:hidden}.barfill{height:100%;background:linear-gradient(90deg,#245843,#84b84f);border-radius:99px}.reportcallout{background:#173d2e;color:white;border-radius:13px;padding:16px;margin-bottom:11px}.reportcallout span{display:block;color:#b9cbc2;font-size:11px}.reportcallout strong{display:block;font-size:25px;margin-top:5px}.mutednote{color:var(--muted);font-size:11px;line-height:1.5}
@media(max-width:900px){html,body{height:100%;overflow:hidden}.shell{height:100vh;grid-template-columns:1fr;overflow:hidden}.sidebar{display:none}.main{height:100vh;padding:14px 16px;overflow:hidden;display:grid;grid-template-rows:auto auto minmax(0,1fr)}.title{font-size:23px}.stats{grid-template-columns:repeat(3,1fr);gap:8px;margin:11px 0}.stat{padding:10px 12px}.stat strong{font-size:21px;margin-top:3px}.stat small{font-size:10px}.grid{min-height:0;grid-template-columns:1fr;grid-template-rows:minmax(0,1fr) auto;gap:9px}.panelhead{padding:10px 14px}.route{padding:18px 7px;overflow:hidden}.track{width:100%;min-width:0}.coach{min-width:0;max-width:none}.coachbox{margin:0 4px;padding:10px 7px 8px;min-height:118px}.coachcode{font-size:12px}.coachtype{font-size:10px;height:27px}.cap{font-size:9px}.coach:not(:last-child):after{right:-1px;width:8px}.coach:not(:last-child):before{right:0}.wheels{margin:7px 4px 0}.wheels i{width:8px;height:8px;box-shadow:16px 0 #536159}.form{padding:9px 11px;display:grid;grid-template-columns:1.05fr 1.15fr .7fr 1.15fr .8fr;gap:7px;align-items:end}.field{margin:0}.field label{font-size:9px;margin-bottom:3px}.field input,.field select{padding:7px 8px;font-size:11px}.btn{padding:8px 5px;font-size:11px}.helper{display:none}.modulebody{padding:10px}.registry{font-size:10px}.registry td{padding:7px 5px}.barrow{grid-template-columns:80px 1fr 30px}}
@media(max-width:520px){.main{padding:10px 8px}.eyebrow{font-size:9px}.title{font-size:20px}.avatar{display:none}.stat{padding:8px}.stathead{font-size:9px}.stat strong{font-size:18px}.stat small{display:none}.panelhead h2{font-size:13px}.ghost,.seg button{font-size:9px;padding:5px}.route{padding:14px 3px}.coachbox{margin:0 2px;padding:8px 5px;min-height:108px}.coachcode{font-size:10px}.coachtype{font-size:8px}.cap{font-size:8px}.remove{font-size:13px;right:4px;top:4px}.wheels i{width:7px;height:7px;box-shadow:12px 0 #536159}.form{grid-template-columns:1fr 1fr 1fr;gap:5px}.form .field:nth-child(4){grid-column:1/3}.btn{grid-column:3}.panelhead span{display:none}}
</style>
</head>
<body><div class="shell">
<aside class="sidebar"><div class="brand"><span class="brandmark"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M6 17h12M7 17l-2 4m12-4 2 4M8 13h8M7 3h10a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2Z"/><circle cx="8" cy="7" r="1" fill="currentColor"/><circle cx="16" cy="7" r="1" fill="currentColor"/></svg></span>RailFlow</div><nav class="nav"><a class="active" href="#coach" onclick="return switchView('coach',this)"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/></svg>Coach Manager</a><a href="#fleet" onclick="return switchView('fleet',this)"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M4 17h16M6 17V7h12v10M9 7V4h6v3"/></svg>Fleet Registry</a><a href="#reports" onclick="return switchView('reports',this)"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M4 19V9m6 10V5m6 14v-7m4 7H2"/></svg>Reports</a></nav><div class="status"><b><span class="dot"></span>Backend online</b>C++ linked-list service<br><span id="host"></span></div></aside>
<main class="main"><header class="top"><div><div class="eyebrow" id="pageEyebrow">Operations workspace</div><h1 class="title" id="pageTitle">Train coach management</h1></div><div class="avatar">OP</div></header>
<section class="stats"><article class="stat"><div class="stathead"><span>Total coaches</span><span>01</span></div><strong id="count">—</strong><small>Live composition</small></article><article class="stat"><div class="stathead"><span>Passenger capacity</span><span>02</span></div><strong id="capacity">—</strong><small>Across all coach types</small></article><article class="stat"><div class="stathead"><span>Data structure</span><span>03</span></div><strong style="font-size:22px">Doubly linked</strong><small>O(1) end insertion</small></article></section>
<section class="grid view active" id="coachView"><article class="panel"><div class="panelhead"><h2>Active composition</h2><div class="actions"><button class="ghost" onclick="resetTrain()">Reset demo</button><div class="seg"><button id="forward" class="active" onclick="setOrder(false)">Forward</button><button id="reverse" onclick="setOrder(true)">Reverse</button></div></div></div><div class="route" id="route"></div></article>
<aside class="panel"><div class="panelhead"><h2>Add a coach</h2><span style="font-size:12px;color:#76817b">New node</span></div><form class="form" id="coachForm"><div class="field"><label for="code">Coach code</label><input id="code" maxlength="12" placeholder="e.g. A2-305" required></div><div class="field"><label for="type">Coach type</label><select id="type"><option>First Class AC</option><option>Sleeper</option><option>Chair Car</option><option>Dining</option><option>Luggage</option><option>Locomotive</option></select></div><div class="field"><label for="capacityInput">Capacity</label><input id="capacityInput" type="number" min="1" max="200" value="64" required></div><div class="field"><label for="position">Insert position</label><select id="position" onchange="toggleAfter()"><option value="back">At the end</option><option value="front">At the beginning</option><option value="after">After selected coach</option></select></div><div class="field" id="afterField" style="display:none"><label for="after">Place after</label><select id="after"></select></div><button class="btn" type="submit">Insert coach</button><div class="helper">Insertion, removal and both traversal directions are executed by the custom C++ doubly linked list.</div></form></aside></section>
<section class="grid view" id="fleetView"><article class="panel"><div class="panelhead"><h2>Coach registry</h2><span style="font-size:12px;color:#76817b">Live linked-list records</span></div><div class="modulebody"><table class="registry"><thead><tr><th>Coach</th><th>Type</th><th>Capacity</th><th>Status</th><th></th></tr></thead><tbody id="registryRows"></tbody></table></div></article><aside class="panel"><div class="panelhead"><h2>Fleet health</h2><span style="font-size:12px;color:#76817b">Real time</span></div><div class="modulebody"><div class="summarylist" id="fleetSummary"></div></div></aside></section>
<section class="grid view" id="reportsView"><article class="panel"><div class="panelhead"><h2>Capacity by coach type</h2><span style="font-size:12px;color:#76817b">Current composition</span></div><div class="modulebody"><div class="chart" id="typeChart"></div></div></article><aside class="panel"><div class="panelhead"><h2>Operational report</h2><span style="font-size:12px;color:#76817b">Auto-calculated</span></div><div class="modulebody" id="reportSummary"></div></aside></section>
</main></div><div class="toast" id="toast"></div>
<script>
let reversed=false,current=[];const $=id=>document.getElementById(id);$('host').textContent=location.host;
function esc(s){return String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
function notify(message){$('toast').textContent=message;$('toast').classList.add('show');setTimeout(()=>$('toast').classList.remove('show'),1800)}
async function load(){const r=await fetch('/api/coaches?order='+(reversed?'reverse':'forward'));const d=await r.json();current=d.coaches;$('count').textContent=d.count;$('capacity').textContent=d.capacity.toLocaleString();render();syncAfter();renderFleet();renderReports()}
function render(){const route=$('route');if(!current.length){route.innerHTML='<div class="empty">No coaches in this composition yet.</div>';return}route.innerHTML='<div class="track">'+current.map(c=>`<div class="coach"><div class="coachbox"><button class="remove" title="Remove coach" onclick="removeCoach(${c.id})">×</button><div class="coachcode">${esc(c.code)}</div><div class="coachtype">${esc(c.type)}</div><div class="cap">◉ ${c.capacity} capacity</div><div class="wheels"><i></i><i></i></div></div></div>`).join('')+'</div>'}
function switchView(name,link){document.querySelectorAll('.view').forEach(v=>v.classList.remove('active'));$(name+'View').classList.add('active');document.querySelectorAll('.nav a').forEach(a=>a.classList.remove('active'));link.classList.add('active');const labels={coach:['Operations workspace','Train coach management'],fleet:['Asset control','Fleet registry'],reports:['Performance intelligence','Operations reports']};$('pageEyebrow').textContent=labels[name][0];$('pageTitle').textContent=labels[name][1];return false}
function renderFleet(){const total=current.reduce((n,c)=>n+c.capacity,0);$('registryRows').innerHTML=current.map(c=>`<tr><td><span class="codepill">${esc(c.code)}</span></td><td>${esc(c.type)}</td><td>${c.capacity}</td><td><span class="badge">Operational</span></td><td><button class="rowremove" onclick="removeCoach(${c.id})">Remove</button></td></tr>`).join('')||'<tr><td colspan="5">No registered coaches.</td></tr>';$('fleetSummary').innerHTML=`<div class="summaryitem"><span>Registered assets</span><strong>${current.length}</strong></div><div class="summaryitem"><span>Passenger capacity</span><strong>${total.toLocaleString()}</strong></div><div class="summaryitem"><span>Service availability</span><strong>${current.length?'100%':'—'}</strong></div>`}
function renderReports(){const groups={};current.forEach(c=>groups[c.type]=(groups[c.type]||0)+c.capacity);const entries=Object.entries(groups).sort((a,b)=>b[1]-a[1]);const max=Math.max(1,...entries.map(x=>x[1]));$('typeChart').innerHTML=entries.map(([type,value])=>`<div class="barrow"><span class="barlabel">${esc(type)}</span><div class="bartrack"><div class="barfill" style="width:${Math.round(value/max*100)}%"></div></div><strong>${value}</strong></div>`).join('')||'<div class="empty">No data available.</div>';const total=current.reduce((n,c)=>n+c.capacity,0),avg=current.length?Math.round(total/current.length):0,top=entries[0];$('reportSummary').innerHTML=`<div class="reportcallout"><span>Total managed capacity</span><strong>${total.toLocaleString()}</strong></div><div class="summarylist"><div class="summaryitem"><span>Average per coach</span><strong>${avg}</strong></div><div class="summaryitem"><span>Largest category</span><strong style="font-size:15px">${top?esc(top[0]):'—'}</strong></div></div><p class="mutednote">Figures are calculated from the current C++ doubly linked-list traversal.</p>`}
function syncAfter(){const items=reversed?[...current].reverse():current;$('after').innerHTML=items.map(c=>`<option value="${c.id}">${esc(c.code)} — ${esc(c.type)}</option>`).join('')}
function setOrder(value){reversed=value;$('forward').classList.toggle('active',!value);$('reverse').classList.toggle('active',value);load()}
function toggleAfter(){$('afterField').style.display=$('position').value==='after'?'block':'none'}
$('coachForm').addEventListener('submit',async e=>{e.preventDefault();const payload={code:$('code').value.trim().toUpperCase(),type:$('type').value,capacity:Number($('capacityInput').value),position:$('position').value,after:Number($('after').value||0)};const r=await fetch('/api/coaches',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});const d=await r.json();if(!r.ok)return notify(d.error||'Unable to add coach');$('code').value='';notify('Coach inserted successfully');load()});
async function removeCoach(id){const r=await fetch('/api/remove',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id})});if(r.ok){notify('Coach removed');load()}else notify('Coach could not be removed')}
async function resetTrain(){await fetch('/api/reset',{method:'POST'});reversed=false;$('forward').classList.add('active');$('reverse').classList.remove('active');notify('Demo composition restored');load()}
load().then(()=>{const view=location.hash.slice(1);if(['coach','fleet','reports'].includes(view)){const link=document.querySelector(`.nav a[href="#${view}"]`);switchView(view,link)}}).catch(()=>notify('C++ backend is unavailable'));
</script></body></html>)HTML";

struct HttpRequest {
    std::string method;
    std::string target;
    std::string body;
};

static bool receiveRequest(SOCKET client, HttpRequest& req) {
    std::string data;
    char buffer[4096];
    std::size_t headerEnd = std::string::npos;
    int contentLength = 0;
    while (data.size() < 1024 * 1024) {
        int received = recv(client, buffer, sizeof(buffer), 0);
        if (received <= 0) return false;
        data.append(buffer, received);
        if (headerEnd == std::string::npos) {
            headerEnd = data.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                std::string headers = data.substr(0, headerEnd);
                std::size_t p = headers.find("Content-Length:");
                if (p != std::string::npos) contentLength = std::atoi(headers.c_str() + p + 15);
            }
        }
        if (headerEnd != std::string::npos && data.size() >= headerEnd + 4 + static_cast<std::size_t>(contentLength)) break;
    }
    std::istringstream first(data.substr(0, data.find("\r\n")));
    first >> req.method >> req.target;
    req.body = headerEnd == std::string::npos ? "" : data.substr(headerEnd + 4, contentLength);
    return !req.method.empty();
}

static void sendAll(SOCKET client, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        int n = send(client, data.data() + sent, static_cast<int>(data.size() - sent), 0);
        if (n <= 0) break;
        sent += static_cast<std::size_t>(n);
    }
}

static void respond(SOCKET client, int status, const std::string& type, const std::string& body) {
    const char* label = status == 200 ? "OK" : status == 201 ? "Created" : status == 400 ? "Bad Request" : "Not Found";
    std::ostringstream out;
    out << "HTTP/1.1 " << status << ' ' << label << "\r\nContent-Type: " << type
        << "\r\nContent-Length: " << body.size() << "\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n" << body;
    sendAll(client, out.str());
}

static void routeRequest(SOCKET client, const HttpRequest& req) {
    if (req.method == "GET" && (req.target == "/" || req.target == "/index.html")) {
        respond(client, 200, "text/html; charset=utf-8", kPage);
    } else if (req.method == "GET" && req.target.rfind("/api/coaches", 0) == 0) {
        respond(client, 200, "application/json", coachJson(req.target.find("reverse") != std::string::npos));
    } else if (req.method == "POST" && req.target == "/api/coaches") {
        std::string code = jsonString(req.body, "code");
        std::string type = jsonString(req.body, "type");
        std::string position = jsonString(req.body, "position");
        int capacity = jsonInt(req.body, "capacity");
        int after = jsonInt(req.body, "after");
        if (code.empty() || type.empty() || capacity < 1 || capacity > 200) {
            respond(client, 400, "application/json", "{\"error\":\"Enter a valid code, type and capacity (1-200).\"}");
            return;
        }
        int id;
        {
            std::lock_guard<std::mutex> lock(gMutex);
            if (position == "front") id = gCoaches.pushFront(code, type, capacity);
            else if (position == "after") id = gCoaches.insertAfter(after, code, type, capacity);
            else id = gCoaches.pushBack(code, type, capacity);
        }
        if (id < 0) respond(client, 400, "application/json", "{\"error\":\"The selected coach no longer exists.\"}");
        else respond(client, 201, "application/json", "{\"ok\":true,\"id\":" + std::to_string(id) + "}");
    } else if (req.method == "POST" && req.target == "/api/remove") {
        bool removed;
        {
            std::lock_guard<std::mutex> lock(gMutex);
            removed = gCoaches.remove(jsonInt(req.body, "id"));
        }
        respond(client, removed ? 200 : 404, "application/json", removed ? "{\"ok\":true}" : "{\"error\":\"Coach not found.\"}");
    } else if (req.method == "POST" && req.target == "/api/reset") {
        {
            std::lock_guard<std::mutex> lock(gMutex);
            gCoaches.seed();
        }
        respond(client, 200, "application/json", "{\"ok\":true}");
    } else {
        respond(client, 404, "application/json", "{\"error\":\"Not found.\"}");
    }
}

int main() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "Could not initialize Winsock.\n";
        return 1;
    }
#endif
    gCoaches.seed();
    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        std::cerr << "Could not create server socket.\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    int reuse = 1;
#ifdef _WIN32
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#else
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
    int port = 8080;
    if (const char* assignedPort = std::getenv("PORT")) {
        int parsed = std::atoi(assignedPort);
        if (parsed > 0 && parsed <= 65535) port = parsed;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<unsigned short>(port));
    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR || listen(server, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Could not start HTTP server on port " << port << ".\n";
        closesocket(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    std::cout << "RailFlow is running on port " << port << "\n";
    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;
        HttpRequest request;
        if (receiveRequest(client, request)) routeRequest(client, request);
        shutdown(client, SD_BOTH);
        closesocket(client);
    }
    closesocket(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
