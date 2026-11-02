#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

using namespace std;

struct Coach {
    string code;
    string type;
    int capacity;
};

struct Node {
    Coach data;
    Node* previous;
    Node* next;

    explicit Node(const Coach& coach)
        : data(coach), previous(nullptr), next(nullptr) {}
};

class TrainCoachList {
private:
    Node* head;
    Node* tail;
    int coachCount;

    Node* findCoach(const string& code) const {
        Node* current = head;
        while (current != nullptr) {
            if (current->data.code == code) {
                return current;
            }
            current = current->next;
        }
        return nullptr;
    }

public:
    TrainCoachList() : head(nullptr), tail(nullptr), coachCount(0) {}

    ~TrainCoachList() {
        clear();
    }

    TrainCoachList(const TrainCoachList&) = delete;
    TrainCoachList& operator=(const TrainCoachList&) = delete;

    bool contains(const string& code) const {
        return findCoach(code) != nullptr;
    }

    bool insertAtBeginning(const Coach& coach) {
        if (contains(coach.code)) {
            return false;
        }

        Node* newNode = new Node(coach);
        newNode->next = head;

        if (head != nullptr) {
            head->previous = newNode;
        } else {
            tail = newNode;
        }

        head = newNode;
        ++coachCount;
        return true;
    }

    bool insertAtEnd(const Coach& coach) {
        if (contains(coach.code)) {
            return false;
        }

        Node* newNode = new Node(coach);
        newNode->previous = tail;

        if (tail != nullptr) {
            tail->next = newNode;
        } else {
            head = newNode;
        }

        tail = newNode;
        ++coachCount;
        return true;
    }

    bool insertAfter(const string& existingCode, const Coach& coach) {
        if (contains(coach.code)) {
            return false;
        }

        Node* current = findCoach(existingCode);
        if (current == nullptr) {
            return false;
        }

        Node* newNode = new Node(coach);
        newNode->previous = current;
        newNode->next = current->next;

        if (current->next != nullptr) {
            current->next->previous = newNode;
        } else {
            tail = newNode;
        }

        current->next = newNode;
        ++coachCount;
        return true;
    }

    bool removeCoach(const string& code) {
        Node* current = findCoach(code);
        if (current == nullptr) {
            return false;
        }

        if (current->previous != nullptr) {
            current->previous->next = current->next;
        } else {
            head = current->next;
        }

        if (current->next != nullptr) {
            current->next->previous = current->previous;
        } else {
            tail = current->previous;
        }

        delete current;
        --coachCount;
        return true;
    }

    void displayForward() const {
        if (head == nullptr) {
            cout << "\nNo coaches are attached to the train.\n";
            return;
        }

        cout << "\nForward traversal (Engine to Rear)\n";
        cout << left << setw(6) << "No." << setw(15) << "Coach Code"
             << setw(24) << "Coach Type" << "Capacity\n";
        cout << string(58, '-') << '\n';

        Node* current = head;
        int position = 1;
        while (current != nullptr) {
            cout << left << setw(6) << position++
                 << setw(15) << current->data.code
                 << setw(24) << current->data.type
                 << current->data.capacity << '\n';
            current = current->next;
        }
    }

    void displayReverse() const {
        if (tail == nullptr) {
            cout << "\nNo coaches are attached to the train.\n";
            return;
        }

        cout << "\nReverse traversal (Rear to Engine)\n";
        cout << left << setw(6) << "No." << setw(15) << "Coach Code"
             << setw(24) << "Coach Type" << "Capacity\n";
        cout << string(58, '-') << '\n';

        Node* current = tail;
        int position = coachCount;
        while (current != nullptr) {
            cout << left << setw(6) << position--
                 << setw(15) << current->data.code
                 << setw(24) << current->data.type
                 << current->data.capacity << '\n';
            current = current->previous;
        }
    }

    void displaySummary() const {
        int totalCapacity = 0;
        Node* current = head;
        while (current != nullptr) {
            totalCapacity += current->data.capacity;
            current = current->next;
        }

        cout << "\nTrain Summary\n";
        cout << "-------------\n";
        cout << "Total coaches    : " << coachCount << '\n';
        cout << "Passenger capacity: " << totalCapacity << '\n';
        cout << "Data structure   : Doubly linked list\n";
    }

    void clear() {
        Node* current = head;
        while (current != nullptr) {
            Node* nextNode = current->next;
            delete current;
            current = nextNode;
        }
        head = nullptr;
        tail = nullptr;
        coachCount = 0;
    }
};

int readInteger(const string& prompt, int minimum, int maximum) {
    int value;
    while (true) {
        cout << prompt;
        if (cin >> value && value >= minimum && value <= maximum) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return value;
        }

        cout << "Enter a number from " << minimum << " to " << maximum << ".\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

string readRequiredText(const string& prompt) {
    string value;
    while (value.empty()) {
        cout << prompt;
        getline(cin, value);
        if (value.empty()) {
            cout << "This value cannot be empty.\n";
        }
    }
    return value;
}

Coach readCoach() {
    Coach coach;
    coach.code = readRequiredText("Coach code: ");
    coach.type = readRequiredText("Coach type: ");
    coach.capacity = readInteger("Capacity (0-500): ", 0, 500);
    return coach;
}

void loadDemoTrain(TrainCoachList& train) {
    train.clear();
    train.insertAtEnd({"ENG-01", "Locomotive", 2});
    train.insertAtEnd({"A1-204", "First Class AC", 48});
    train.insertAtEnd({"B2-118", "Sleeper", 72});
    train.insertAtEnd({"D1-076", "Dining", 36});
    train.insertAtEnd({"LUG-09", "Luggage", 12});
}

void showMenu() {
    cout << "\n========================================\n";
    cout << "       TRAIN COACH MANAGEMENT SYSTEM\n";
    cout << "========================================\n";
    cout << "1. Insert coach at beginning\n";
    cout << "2. Insert coach at end\n";
    cout << "3. Insert coach after another coach\n";
    cout << "4. Remove coach\n";
    cout << "5. Traverse forward\n";
    cout << "6. Traverse reverse\n";
    cout << "7. Show train summary\n";
    cout << "8. Reset demo train\n";
    cout << "0. Exit\n";
}

int main() {
    TrainCoachList train;
    loadDemoTrain(train);

    cout << "Train Coach Management System started.\n";

    while (true) {
        showMenu();
        int choice = readInteger("Choose an option: ", 0, 8);

        if (choice == 0) {
            cout << "Program closed.\n";
            break;
        }

        if (choice == 1 || choice == 2) {
            Coach coach = readCoach();
            bool inserted = (choice == 1)
                                ? train.insertAtBeginning(coach)
                                : train.insertAtEnd(coach);
            cout << (inserted ? "Coach inserted successfully.\n"
                              : "A coach with that code already exists.\n");
        } else if (choice == 3) {
            string existingCode = readRequiredText("Insert after coach code: ");
            Coach coach = readCoach();
            bool inserted = train.insertAfter(existingCode, coach);
            cout << (inserted
                         ? "Coach inserted successfully.\n"
                         : "Insertion failed. Check that the codes are valid and unique.\n");
        } else if (choice == 4) {
            string code = readRequiredText("Coach code to remove: ");
            cout << (train.removeCoach(code) ? "Coach removed successfully.\n"
                                             : "Coach not found.\n");
        } else if (choice == 5) {
            train.displayForward();
        } else if (choice == 6) {
            train.displayReverse();
        } else if (choice == 7) {
            train.displaySummary();
        } else if (choice == 8) {
            loadDemoTrain(train);
            cout << "Demo train restored.\n";
        }
    }

    return 0;
}
