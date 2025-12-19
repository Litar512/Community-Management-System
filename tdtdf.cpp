/**
 * 项目名称：小区物业管理系统 (基于严蔚敏《数据结构》课程设计)
 * 开发语言：C++
 * 数据结构设计说明：
 * 1. 住户信息：单链表 (LinkedList) - 便于动态增删。
 * 2. 房号索引：二叉排序树 (BST) - 满足"快速查询住户房号"的要求，存储房号与住户节点的指针映射。
 * 3. 报修队列：链式队列 (LinkQueue) - 满足"按顺序分配"的FIFO特性 (待分配任务)。
 * 4. 维修历史：单链表 (LinkedList) - 存储处理中和已完成的报修任务，便于后续评价。
 * 5. 社区活动：树 (孩子兄弟表示法) - 管理活动分类和层级。
 * 6. 缴费记录：子链表 - 挂载在每个住户节点下。
 * 7. 物业人员：单链表 (LinkedList) - 管理人员信息及工作分配。
 */

#include <iostream>
#include <string>
#include <iomanip>
#include <vector>
#include <cstdlib> // 用于 system() 函数
#include <fstream> // 用于文件操作

using namespace std;

// ==========================================
// 1. 基础数据结构定义
// ==========================================

// --- 缴费记录节点 (子链表) ---
struct FeeNode {
    string feeType;   // 费用类型 (物业费/水电费)
    double amount;    // 金额
    bool isPaid;      // 是否已缴
    string date;      // 日期
    FeeNode* next;
};

// --- 住户节点 (主链表) ---
struct ResidentNode {
    string roomNumber; // 房号 (Key)
    string name;       // 姓名
    string phone;      // 电话
    double area;       // 房屋面积
    
    FeeNode* feeHead;  // 指向缴费记录的头指针
    ResidentNode* next;// 链表指针

    ResidentNode() : feeHead(nullptr), next(nullptr) {}
};

// --- BST索引节点 (用于快速查找) ---
// 使用树结构加速查找
struct BSTNode {
    string roomNumber;
    ResidentNode* target; // 指向链表中对应住户的指针
    BSTNode *lchild, *rchild;
};

// --- 报修节点 (队列/历史链表元素) ---
struct RepairNode {
    int id;            // 报修单号
    string roomNumber; // 房号
    string content;    // 内容
    string status;     // 状态 (待分配/处理中/已完成)
    string comment;    // 住户评价
    int rating;        // 评分 (1-5, 0表示未评分)
    RepairNode* next;
};

// --- 报修队列 (仅存储待分配任务) ---
struct RepairQueue {
    RepairNode* front; // 队头
    RepairNode* rear;  // 队尾
};

// --- 活动分类/项目节点 (树：孩子兄弟表示法) ---
struct ActivityNode {
    string name;       // 分类名 或 活动名
    bool isActivity;   // true为具体活动，false为分类文件夹
    string info;       // 活动详情 (如果是活动)
    int participants;  // 报名人数
    
    ActivityNode* firstChild; // 第一个孩子 (下一级)
    ActivityNode* nextSibling;// 下一个兄弟 (同级)
};

// --- 物业人员节点 (扩展) ---
struct StaffNode {
    string id;          // 工号
    string name;        // 姓名
    int age;            // 年龄
    string phone;       // 电话
    string idCard;      // 身份证号
    string level;       // 级别 (如: 经理/维修工/保安)
    string job;         // 工作分配
    StaffNode* next;
};

// ==========================================
// 2. 系统类定义
// ==========================================

class PropertySystem {
private:
    const int MAX_CAPACITY = 100; // 模拟小区总户数，用于计算入住率

    ResidentNode* resHead;        // 住户链表头
    BSTNode* resIndexRoot;        // 房号索引树根
    RepairQueue repairQ;          // 待分配报修队列
    RepairNode* repairHistoryHead;// [新增] 报修历史链表头(存储处理中/已完成)
    ActivityNode* actRoot;        // 活动树根
    StaffNode* staffHead;         // 员工链表
    int repairCounter;            // 报修单计数器

    // --- 内部辅助函数 ---

    // BST 插入 (建立索引)
    void insertBST(BSTNode*& root, string room, ResidentNode* target) {
        if (!root) {
            root = new BSTNode{room, target, nullptr, nullptr};
        } else if (room < root->roomNumber) {
            insertBST(root->lchild, room, target);
        } else if (room > root->roomNumber) {
            insertBST(root->rchild, room, target);
        }
    }

    // BST 查找
    ResidentNode* searchBST(BSTNode* root, string room) {
        if (!root) return nullptr;
        if (room == root->roomNumber) return root->target;
        if (room < root->roomNumber) return searchBST(root->lchild, room);
        return searchBST(root->rchild, room);
    }

    // 辅助：销毁BST (用于删除住户后重建索引)
    void destroyBST(BSTNode*& root) {
        if (!root) return;
        destroyBST(root->lchild);
        destroyBST(root->rchild);
        delete root;
        root = nullptr;
    }

    // 辅助：重建索引
    void rebuildIndex() {
        destroyBST(resIndexRoot);
        ResidentNode* p = resHead->next;
        while (p) {
            insertBST(resIndexRoot, p->roomNumber, p);
            p = p->next;
        }
    }

    // 树的先序遍历 (打印活动)
    void printActivityTree(ActivityNode* node, int level) {
        if (!node) return;
        
        for (int i = 0; i < level; ++i) cout << "  "; // 缩进显示层级
        cout << (node->isActivity ? "[活动] " : "[分类] ") << node->name;
        if (node->isActivity) {
            cout << " (详情: " << node->info << ", 报名: " << node->participants << "人)";
        }
        cout << endl;

        printActivityTree(node->firstChild, level + 1);
        printActivityTree(node->nextSibling, level);
    }

    // [新增] 辅助遍历生成报告 (递归)
    void printReportRecursive(ActivityNode* node, int& count, int& people) {
        if (!node) return;
        if (node->isActivity) {
            cout << left << setw(20) << node->name 
                 << setw(30) << (node->info.empty() ? "-" : node->info) 
                 << setw(10) << node->participants << endl;
            count++;
            people += node->participants;
        }
        printReportRecursive(node->firstChild, count, people);
        printReportRecursive(node->nextSibling, count, people);
    }

    // 查找活动节点的辅助函数
    ActivityNode* findActivityNode(ActivityNode* node, string name) {
        if (!node) return nullptr;
        if (node->name == name) return node;
        ActivityNode* found = findActivityNode(node->firstChild, name);
        if (found) return found;
        return findActivityNode(node->nextSibling, name);
    }

    // --- 辅助文件保存函数 (递归保存树) ---
    void saveActivityNodeRecursive(ofstream& outFile, ActivityNode* node, string parentName) {
        if (!node) return;
        
        string safeInfo = (node->info.empty() ? "-" : node->info); 
        outFile << parentName << " " 
                << node->name << " " 
                << node->isActivity << " " 
                << safeInfo << " " 
                << node->participants << endl;

        saveActivityNodeRecursive(outFile, node->firstChild, node->name);
        saveActivityNodeRecursive(outFile, node->nextSibling, parentName);
    }

    bool bpR(string s){
    	StaffNode* p = staffHead->next;
    	
        //if(!p) { cout << "暂无员工信息。" << endl; return false; }
        while(p) {
        	if(p->name==s&&p->level=="维修工"){
        		return true;
			}
            
            p = p->next;
        }
        return false;
	}
	
	bool nopeople(){
		StaffNode* p = staffHead->next;
		if(!p) {  return true; }
		while(p) {
        	if(p->level=="维修工"){
        		return false;
			}
            
            p = p->next;
        }
		return true;
	}

public:
    PropertySystem() {
        // 初始化住户链表
        resHead = new ResidentNode(); 
        resIndexRoot = nullptr;

        // 初始化队列
        repairQ.front = repairQ.rear = new RepairNode();
        repairQ.front->next = nullptr;
        repairCounter = 1000;
        
        // 初始化历史链表
        repairHistoryHead = new RepairNode();
        repairHistoryHead->next = nullptr;

        // 初始化活动树
        actRoot = new ActivityNode{"社区活动中心", false, "", 0, nullptr, nullptr};
        
        // 初始化员工链表
        staffHead = new StaffNode(); 
        
        loadAllData();
    }

    // ================== 文件持久化模块 ==================

    void saveResidents() {
        ofstream outFile("residents.txt");
        if (!outFile) return;
        ResidentNode* p = resHead->next;
        while (p) {
            outFile << p->roomNumber << " " << p->name << " " << p->phone << " " << p->area << endl;
            p = p->next;
        }
        outFile.close();
        saveFees(); 
    }

    void saveFees() {
        ofstream outFile("fees.txt");
        if (!outFile) return;
        ResidentNode* p = resHead->next;
        while (p) {
            FeeNode* f = p->feeHead;
            while (f) {
                outFile << p->roomNumber << " " << f->feeType << " " << f->amount << " " << f->isPaid << " " << f->date << endl;
                f = f->next;
            }
            p = p->next;
        }
        outFile.close();
    }

    void saveRepairs() {
        ofstream outFile("repairs.txt");
        if (!outFile) return;
        
        // 1. 保存队列中的任务 (待分配)
        RepairNode* p = repairQ.front->next;
        while (p) {
            string safeComment = (p->comment.empty() ? "-" : p->comment);
            outFile << p->id << " " << p->roomNumber << " " << p->content << " " << p->status << " " << p->rating << " " << safeComment << endl;
            p = p->next;
        }
        
        // 2. 保存历史/处理中任务
        p = repairHistoryHead->next;
        while (p) {
            string safeComment = (p->comment.empty() ? "-" : p->comment);
            outFile << p->id << " " << p->roomNumber << " " << p->content << " " << p->status << " " << p->rating << " " << safeComment << endl;
            p = p->next;
        }

        outFile.close();
        
        ofstream countFile("repair_count.txt");
        countFile << repairCounter;
        countFile.close();
    }

    void saveActivities() {
        ofstream outFile("activities.txt");
        if (!outFile) return;
        saveActivityNodeRecursive(outFile, actRoot->firstChild, actRoot->name);
        outFile.close();
    }

    void saveStaff() {
        ofstream outFile("staff.txt");
        if(!outFile) return;
        StaffNode* p = staffHead->next;
        while(p) {
            outFile << p->id << " " << p->name << " " << p->age << " " 
                    << p->phone << " " << p->idCard << " " << p->level << " " << p->job << endl;
            p = p->next;
        }
        outFile.close();
    }

    void loadAllData() {
        bool hasData = false;

        // 1. 加载住户
        ifstream resFile("residents.txt");
        if (resFile) {
            string r, n, p; double a;
            while (resFile >> r >> n >> p >> a) {
                ResidentNode* node = new ResidentNode();
                node->roomNumber = r; node->name = n; node->phone = p; node->area = a;
                node->next = resHead->next; resHead->next = node;
                insertBST(resIndexRoot, r, node);
                hasData = true;
            }
            resFile.close();
        }

        // 2. 加载费用
        ifstream feeFile("fees.txt");
        if (feeFile) {
            string r, type, date; double amt; bool paid;
            while (feeFile >> r >> type >> amt >> paid >> date) {
                ResidentNode* target = searchBST(resIndexRoot, r);
                if (target) {
                    FeeNode* f = new FeeNode{type, amt, paid, date, target->feeHead};
                    target->feeHead = f;
                }
            }
            feeFile.close();
        }

        // 3. 加载报修
        ifstream repFile("repairs.txt");
        if (repFile) {
            int id, rating; string r, c, s, comment;
            while (repFile >> id >> r >> c >> s >> rating >> comment) {
                if (comment == "-") comment = "";

                RepairNode* node = new RepairNode();
                node->id = id; node->roomNumber = r; node->content = c; 
                node->status = s; node->rating = rating; node->comment = comment;
                node->next = nullptr;

                if (s == "待分配") {
                    // 入队
                    repairQ.rear->next = node;
                    repairQ.rear = node;
                } else {
                    // 入历史链表 (头插法)
                    node->next = repairHistoryHead->next;
                    repairHistoryHead->next = node;
                }
            }
            repFile.close();
        }
        ifstream countFile("repair_count.txt");
        if (countFile) { countFile >> repairCounter; countFile.close(); }

        // 4. 加载活动
        ifstream actFile("activities.txt");
        if (actFile) {
            string pName, name, info; bool isAct; int parts;
            while (actFile >> pName >> name >> isAct >> info >> parts) {
                if (info == "-") info = ""; 
                ActivityNode* parent = findActivityNode(actRoot, pName);
                if (parent) {
                    ActivityNode* newNode = new ActivityNode{name, isAct, info, parts, nullptr, nullptr};
                    newNode->nextSibling = parent->firstChild;
                    parent->firstChild = newNode;
                }
            }
            actFile.close();
        }

        // 5. 加载员工
        ifstream staffFile("staff.txt");
        if(staffFile) {
            string id, name, phone, idc, lvl, job; int age;
            while(staffFile >> id >> name >> age >> phone >> idc >> lvl >> job) {
                StaffNode* s = new StaffNode();
                s->id = id; s->name = name; s->age = age;
                s->phone = phone; s->idCard = idc;
                s->level = lvl; s->job = job;
                s->next = staffHead->next;
                staffHead->next = s;
            }
            staffFile.close();
        }

        if (!hasData) {
            preLoadData();
            saveResidents();
            saveActivities();
            saveStaff();
        }
    }

    void preLoadData() {
        ResidentNode* r1 = new ResidentNode();
        r1->roomNumber = "101"; r1->name = "张三"; r1->phone = "1380001"; r1->area = 90.5;
        r1->next = resHead->next; resHead->next = r1;
        insertBST(resIndexRoot, "101", r1);

        ResidentNode* r2 = new ResidentNode();
        r2->roomNumber = "102"; r2->name = "李四"; r2->phone = "1380002"; r2->area = 120.0;
        r2->next = resHead->next; resHead->next = r2;
        insertBST(resIndexRoot, "102", r2);

        ActivityNode* sports = new ActivityNode{"体育类", false, "", 0, nullptr, nullptr};
        actRoot->firstChild = sports;
        ActivityNode* basket = new ActivityNode{"秋季篮球赛", true, "周六下午", 5, nullptr, nullptr};
        sports->firstChild = basket;
        
        // 预设一个员工
        StaffNode* s = new StaffNode();
        s->id = "S001"; s->name = "王管家"; s->age = 35; 
        s->phone = "18900001111"; s->idCard = "320102199001011234";
        s->level = "经理"; s->job = "全面负责";
        s->next = staffHead->next; staffHead->next = s;
        
        StaffNode* repairman = new StaffNode();
    repairman->id = "S002"; 
    repairman->name = "李维修"; 
    repairman->age = 28;
    repairman->phone = "18900002222"; 
    repairman->idCard = "320102199501012345";
    repairman->level = "维修工";  // 注意：这里的级别需要与processRepair函数中检查的级别一致
    repairman->job = "维修";
    
    // 头插法添加到员工链表
    repairman->next = staffHead->next;
    staffHead->next = repairman;
    }
    
    
    // ================== 1. 住户管理模块 ==================

    void addResident(string room, string name, string phone, double area) {
        if (searchBST(resIndexRoot, room) != nullptr) {
            cout << "错误：该房号已存在！" << endl;
            return;
        }
        ResidentNode* p = new ResidentNode();
        p->roomNumber = room; p->name = name; p->phone = phone; p->area = area;
        p->next = resHead->next; resHead->next = p;
        insertBST(resIndexRoot, room, p);
        cout << "住户添加成功！" << endl;
        saveResidents();
    }

    void modifyResident() {
        string room;
        cout << "请输入要修改的住户房号: "; cin >> room;
        ResidentNode* p = searchBST(resIndexRoot, room);
        if (!p) { cout << "未找到该住户！" << endl; return; }
        
        cout << "当前信息 -> 姓名: " << p->name << " 电话: " << p->phone << " 面积: " << p->area << endl;
        cout << "输入新姓名(不改输入-): "; string n; cin >> n;
        if (n != "-") p->name = n;
        cout << "输入新电话(不改输入-): "; string ph; cin >> ph;
        if (ph != "-") p->phone = ph;
        cout << "输入新面积(不改输入0): "; double a; cin >> a;
        if (a != 0) p->area = a;

        cout << "修改成功！" << endl;
        saveResidents();
    }

    void deleteResident() {
        string room;
        cout << "请输入要删除的住户房号: "; cin >> room;
        ResidentNode* p = resHead;
        ResidentNode* q = nullptr;
        bool found = false;
        while (p->next) {
            if (p->next->roomNumber == room) {
                q = p->next;
                p->next = q->next;
                delete q; 
                found = true;
                break;
            }
            p = p->next;
        }
        if (found) {
            rebuildIndex();
            cout << "删除成功，索引已更新。" << endl;
            saveResidents();
        } else {
            cout << "未找到该房号的住户。" << endl;
        }
    }

    void searchResident() {
        string room;
        cout << "请输入房号查询: "; cin >> room;
        ResidentNode* p = searchBST(resIndexRoot, room);
        if (p) {
            cout << "\n--- 住户信息 ---" << endl;
            cout << "房号: " << p->roomNumber << endl;
            cout << "姓名: " << p->name << endl;
            cout << "电话: " << p->phone << endl;
            cout << "面积: " << p->area << " 平方米" << endl;
        } else {
            cout << "未找到该房号的住户。" << endl;
        }
    }

    void showStatsAndResidents() {
        ResidentNode* p = resHead->next;
        int count = 0;
        cout << "\n--- 小区住户列表 ---" << endl;
        cout << left << setw(10) << "房号" << setw(10) << "姓名" << setw(15) << "电话" << endl;
        while (p) {
            cout << left << setw(10) << p->roomNumber << setw(10) << p->name << setw(15) << p->phone << endl;
            p = p->next;
            count++;
        }
        double occupancyRate = (count > 0) ? ((double)count / MAX_CAPACITY) * 100.0 : 0.0;
        cout << "\n--- 小区统计信息 ---" << endl;
        cout << "当前住户总数: " << count << " 户" << endl;
        cout << "小区规划总数: " << MAX_CAPACITY << " 户" << endl;
        cout << "当前入住率  : " << fixed << setprecision(2) << occupancyRate << "%" << endl;
    }

    void showAllResidents() { showStatsAndResidents(); }

    // ================== 2. 物业缴费模块 ==================

    void addFee() {
        string room;
        cout << "输入房号以生成账单: "; cin >> room;
        ResidentNode* p = searchBST(resIndexRoot, room);
        if (!p) { cout << "住户不存在。" << endl; return; }

        FeeNode* f = new FeeNode();
        cout << "输入费用类型: "; cin >> f->feeType;
        cout << "输入金额: "; cin >> f->amount;
        f->isPaid = false;
        f->date = "2023-10-01"; 
        f->next = p->feeHead; p->feeHead = f;
        cout << "账单生成成功！" << endl;
        saveFees();
    }

    void payFee() {
        string room;
        cout << "输入房号进行缴费: "; cin >> room;
        ResidentNode* p = searchBST(resIndexRoot, room);
        if (!p) { cout << "住户不存在。" << endl; return; }

        FeeNode* f = p->feeHead;
        bool found = false;
        while (f) {
            if (!f->isPaid) {
                cout << "发现未缴账单: " << f->feeType << " 金额: " << f->amount << endl;
                cout << "是否缴费? (y/n): ";
                char ch; cin >> ch;
                if (ch == 'y' || ch == 'Y') {
                    f->isPaid = true;
                    cout << "缴费成功！" << endl;
                    saveFees(); 
                }
                found = true;
            }
            f = f->next;
        }
        if (!found) cout << "该住户没有未缴账单。" << endl;
    }

    void showFeeHistory() {
        string room;
        cout << "请输入房号查询缴费记录: "; cin >> room;
        ResidentNode* p = searchBST(resIndexRoot, room);
        if (!p) { cout << "住户不存在。" << endl; return; }

        FeeNode* f = p->feeHead;
        if (!f) { cout << "该住户暂无缴费记录。" << endl; return; }

        cout << "\n--- 缴费历史记录 (" << p->name << ") ---" << endl;
        cout << left << setw(15) << "费用类型" << setw(10) << "金额" << setw(10) << "状态" << setw(15) << "日期" << endl;
        while (f) {
            cout << left << setw(15) << f->feeType << setw(10) << f->amount << setw(10) << (f->isPaid ? "已缴" : "未缴") << setw(15) << f->date << endl;
            f = f->next;
        }
    }

    // ================== 3. 报修服务模块 ==================

    void submitRepair() {
        string room, content;
        cout << "输入房号: "; cin >> room;
        if (!searchBST(resIndexRoot, room)) { cout << "非本小区住户。" << endl; return; }
        
        cout << "输入报修内容(无空格): "; cin >> content;

        RepairNode* r = new RepairNode();
        r->id = ++repairCounter;
        r->roomNumber = room;
        r->content = content;
        r->status = "待分配";
        r->comment = ""; 
        r->rating = 0;
        r->next = nullptr;

        repairQ.rear->next = r;
        repairQ.rear = r;

        cout << "报修提交成功，单号: " << r->id << endl;
        saveRepairs(); 
    }

    // 分配维修人员 (更新进度：待分配 -> 处理中)
    
    void processRepair() {
        if (repairQ.front == repairQ.rear) {
            cout << "当前没有待处理的报修任务。" << endl;
            return;
        }
		string worker;
        RepairNode* p = repairQ.front->next; // 获取队首
        cout << "\n--- 处理报修任务 ---" << endl;
        cout << "单号: " << p->id << " | 房号: " << p->roomNumber << " | 内容: " << p->content << endl;
        showAllStaff();
        if(nopeople()){
        	cout<<"暂无维修工";
        	return ;
		}
		
        while(true){
        	cout << "分配维修人员 (输入姓名，无空格): ";
        	 cin >> worker;
        	if(bpR(worker))break;
        	cout<<"请输入有效维修工!!!\n";
		}
		
        
        // 更新状态
        p->status = "处理中:" + worker; 
        
        // 出队
        repairQ.front->next = p->next;
        if (repairQ.rear == p) repairQ.rear = repairQ.front;

        // 入历史链表 (用于跟踪进度和评价)
        p->next = repairHistoryHead->next;
        repairHistoryHead->next = p;
        
        cout << "任务已分配并记录进度！" << endl;
        saveRepairs(); 
    }

    // 完成维修 (更新进度：处理中 -> 已完成)
    void finishRepair() {
        int id;
        cout << "请输入已完成的工单ID: "; cin >> id;

        RepairNode* p = repairHistoryHead->next;
        bool found = false;
        while (p) {
            if (p->id == id) {
                // 简单判断，只要不是待分配，就可以设为完成
                p->status = "已完成";
                cout << "工单 " << id << " 状态已更新为：已完成。" << endl;
                found = true;
                break;
            }
            p = p->next;
        }
        if (!found) cout << "未找到该处理中的工单(请确认ID或是否已分配)。" << endl;
        saveRepairs();
    }

    // 住户评价 (评价/打分)
    void evaluateRepair() {
        string room;
        cout << "请输入您的房号: "; cin >> room;
        if (!searchBST(resIndexRoot, room)) { cout << "房号不存在。" << endl; return; }

        cout << "\n--- 您的已完成报修记录 ---" << endl;
        RepairNode* p = repairHistoryHead->next;
        bool found = false;
        while (p) {
            if (p->roomNumber == room && p->status == "已完成") {
                cout << "ID: " << p->id << " 内容: " << p->content 
                     << " 评分: " << (p->rating == 0 ? "未评" : to_string(p->rating) + "星")
                     << " 评价: " << (p->comment.empty() ? "无" : p->comment) << endl;
                found = true;
            }
            p = p->next;
        }

        if (!found) { cout << "暂无已完成的报修记录。" << endl; return; }

        int id;
        cout << "\n请输入要评价的工单ID: "; cin >> id;
        
        p = repairHistoryHead->next;
        while (p) {
            if (p->id == id && p->roomNumber == room && p->status == "已完成") {
                cout << "请输入评分 (1-5): "; cin >> p->rating;
                cout << "请输入评价内容 (无空格): "; cin >> p->comment;
                cout << "评价提交成功！感谢您的反馈。" << endl;
                saveRepairs();
                return;
            }
            p = p->next;
        }
        cout << "工单ID无效或不属于该房间。" << endl;
    }

    // [新增] 查看所有报修任务进度
    void showAllRepairs() {
        cout << "\n--- 所有报修任务进度 ---" << endl;
        
        // 1. 待分配任务 (队列)
        cout << "[待分配任务]" << endl;
        RepairNode* p = repairQ.front->next;
        if (!p) cout << "  (无)" << endl;
        while (p) {
            cout << "  ID:" << p->id << " 房号:" << p->roomNumber 
                 << " 内容:" << p->content << " 状态:" << p->status << endl;
            p = p->next;
        }

        // 2. 历史任务 (处理中/已完成)
        cout << "\n[处理中/已完成任务]" << endl;
        p = repairHistoryHead->next;
        if (!p) cout << "  (无)" << endl;
        while (p) {
            cout << "  ID:" << p->id << " 房号:" << p->roomNumber 
                 << " 内容:" << p->content << " 状态:" << p->status;
            if (p->status == "已完成") {
                 cout << " 评分:" << (p->rating == 0 ? "未评" : to_string(p->rating) + "星");
            }
            cout << endl;
            p = p->next;
        }
        cout << "------------------------" << endl;
    }
    
    // ================== 4. 社区活动模块 ==================

    void addActivity() {
        cout << "--- 当前活动分类结构 ---" << endl;
        printActivityTree(actRoot, 0);

        string parentName, actName, info;
        cout << "\n请输入父节点名称: "; cin >> parentName;
        ActivityNode* parent = findActivityNode(actRoot, parentName);
        if (!parent) { cout << "未找到该分类！" << endl; return; }

        cout << "输入名称: "; cin >> actName;
        cout << "是具体活动吗? (1:是, 0:否): ";
        int type; cin >> type;

        ActivityNode* newNode = new ActivityNode();
        newNode->name = actName;
        newNode->isActivity = (type == 1);
        newNode->participants = 0;
        newNode->firstChild = nullptr; newNode->nextSibling = nullptr;
        
        if (type == 1) {
            cout << "输入详情(无空格): "; cin >> info;
            newNode->info = info;
        } else {
            newNode->info = "";
        }

        newNode->nextSibling = parent->firstChild;
        parent->firstChild = newNode;
        cout << "添加成功！" << endl;
        saveActivities(); 
    }

    // [新增] 住户报名活动
    void joinActivity() {
        string room, actName;
        cout << "请输入您的房号: "; cin >> room;
        // 验证住户身份
        ResidentNode* res = searchBST(resIndexRoot, room);
        if (!res) { cout << "非本小区住户，无法报名。" << endl; return; }

        cout << "\n--- 当前活动列表 ---" << endl;
        printActivityTree(actRoot, 0); // 展示树结构，方便用户查看名称

        cout << "\n请输入要参加的活动名称: "; cin >> actName;
        ActivityNode* act = findActivityNode(actRoot, actName);
        
        if (act && act->isActivity) {
            act->participants++; // 增加人数
            cout << "报名成功！\n活动 <" << act->name << "> 当前已有 " << act->participants << " 人报名。" << endl;
            saveActivities(); // 保存更新
        } else {
            cout << "未找到该活动，或该节点仅为分类目录(不可报名)。" << endl;
        }
    }

    // [新增] 生成活动总结报告
    void generateActivityReport() {
        cout << "\n========================================" << endl;
        cout << "          社区活动总结报告              " << endl;
        cout << "========================================" << endl;
        cout << left << setw(20) << "活动名称" << setw(30) << "详情(时间/地点)" << setw(10) << "参与人数" << endl;
        cout << "------------------------------------------------------------" << endl;
        
        int totalActs = 0;
        int totalPeople = 0;
        
        // 遍历树打印统计
        printReportRecursive(actRoot, totalActs, totalPeople);
        
        cout << "------------------------------------------------------------" << endl;
        cout << "统计汇总: 共举办 " << totalActs << " 场活动，累计参与 " << totalPeople << " 人次。" << endl;
        
        // 生成本地文件
        ofstream rptFile("ActivityReport.txt");
        if(rptFile) {
            rptFile << "社区活动总结报告\n";
            rptFile << "------------------------------------------------------------\n";
            rptFile << left << setw(20) << "活动名称" << setw(30) << "详情" << setw(10) << "人数" << endl;
            
            // 重新遍历一次写入文件 (为了简单复用逻辑，这里手动再做一次或写个带ofstream的递归函数，这里简化处理只写汇总)
            // 如果需要详细写入文件，需要重写一个带ofstream参数的递归函数，这里简化处理只写汇总
            rptFile << "\n(详细列表见控制台输出)\n"; 
            rptFile << "统计汇总: 共举办 " << totalActs << " 场活动，累计参与 " << totalPeople << " 人次。\n";
            rptFile.close();
            cout << "(报告文件已生成至 ActivityReport.txt)" << endl;
        }
    }

    void showActivities() {
        cout << "\n--- 社区活动层级图 ---" << endl;
        printActivityTree(actRoot, 0);
    }

    // [新增] 删除活动/分类
    void deleteActivity() {
        cout << "--- 当前活动分类结构 ---" << endl;
        printActivityTree(actRoot, 0);
        cout << "\n注意：删除分类会同时删除其下所有子活动/子分类！" << endl;

        string targetName;
        cout << "请输入要删除的活动/分类名称: ";
        cin >> targetName;

        // 特殊判断：不允许删除根节点(避免整个活动树被清空)
        if (targetName == actRoot->name) {
            cout << "错误：根分类不可删除！" << endl;
            return;
        }

        // 查找目标节点及其父节点(需要自定义辅助函数)
        ActivityNode *parent = nullptr;
        ActivityNode *target = findActivityNodeWithParent(actRoot, targetName, parent);

        if (!target) {
            cout << "未找到名称为 <" << targetName << "> 的活动/分类！" << endl;
            return;
        }

        // 确认删除
        cout << "确认删除";
        if (target->isActivity) {
            cout << "活动 <" << targetName << ">";
        } else {
            cout << "分类 <" << targetName << "> (含其下所有子节点)";
        }
        cout << "? (1:确认, 0:取消): ";
        int confirm;
        cin >> confirm;
        if (confirm != 1) {
            cout << "删除已取消。" << endl;
            return;
        }

        // 从父节点的子链表中移除目标节点
        if (parent->firstChild == target) {
            // 目标是父节点的第一个子节点
            parent->firstChild = target->nextSibling;
        } else {
            // 目标是父节点的非第一个子节点，遍历查找前驱节点
            ActivityNode *prevSibling = parent->firstChild;
            while (prevSibling != nullptr && prevSibling->nextSibling != target) {
                prevSibling = prevSibling->nextSibling;
            }
            if (prevSibling != nullptr) {
                prevSibling->nextSibling = target->nextSibling;
            }
        }

        // 递归删除目标节点及其所有子节点(释放内存)
        deleteActivityTree(target);

        cout << "删除成功！" << endl;
        saveActivities(); // 保存删除后的结构
    }

    // [辅助函数] 查找目标节点，并返回其父节点
    ActivityNode* findActivityNodeWithParent(ActivityNode *root, const string &name, ActivityNode *&parent) {
        if (root == nullptr) return nullptr;

        // 遍历当前节点的所有子节点
        ActivityNode *current = root->firstChild;
        while (current != nullptr) {
            if (current->name == name) {
                parent = root; // 找到目标，记录父节点
                return current;
            }
            // 递归查找子节点的子树
            ActivityNode *found = findActivityNodeWithParent(current, name, parent);
            if (found != nullptr) {
                return found;
            }
            current = current->nextSibling;
        }
        return nullptr;
    }

    // [辅助函数] 递归删除节点及其所有子节点(释放内存)
    void deleteActivityTree(ActivityNode *node) {
        if (node == nullptr) return;

        // 递归删除所有子节点
        deleteActivityTree(node->firstChild);
        // 删除当前节点的下一个兄弟节点
        deleteActivityTree(node->nextSibling);
        // 释放当前节点内存
        delete node;
    }

    // ================== 5. 物业人员管理模块 ==================

    void addStaffMember() {
        string id, name, phone, idCard, level;
        int age;
        cout << "请输入工号: "; cin >> id;
        
        // 查重
        StaffNode* p = staffHead->next;
        while(p) {
            if(p->id == id) { cout << "工号已存在!" << endl; return; }
            p = p->next;
        }

        cout << "姓名: "; cin >> name;
        cout << "年龄: "; cin >> age;
        cout << "电话: "; cin >> phone;
        cout << "身份证号: "; cin >> idCard;
        cout << "级别(如:经理/保安/保洁/维修工): "; cin >> level;

        StaffNode* s = new StaffNode();
        s->id = id; s->name = name; s->age = age;
        s->phone = phone; s->idCard = idCard;
        s->level = level; s->job = "暂无分配";
        
        // 头插法
        s->next = staffHead->next;
        staffHead->next = s;
        
        cout << "员工添加成功!" << endl;
        saveStaff();
    }

    void modifyStaff() {
        string id;
        cout << "输入要修改的员工工号: "; cin >> id;
        StaffNode* p = staffHead->next;
        while(p) {
            if(p->id == id) {
                cout << "当前信息: " << p->name << " " << p->level << endl;
                cout << "新姓名(-保持不变): "; string v; cin >> v; if(v!="-") p->name = v;
                cout << "新年龄(0保持不变): "; int a; cin >> a; if(a!=0) p->age = a;
                cout << "新电话(-保持不变): "; cin >> v; if(v!="-") p->phone = v;
                cout << "新级别(-保持不变): "; cin >> v; if(v!="-") p->level = v;
                cout << "修改成功!" << endl;
                saveStaff();
                return;
            }
            p = p->next;
        }
        cout << "未找到该员工。" << endl;
    }

    void deleteStaff() {
        string id;
        cout << "输入要删除的员工工号: "; cin >> id;
        StaffNode* p = staffHead;
        while(p->next) {
            if(p->next->id == id) {
                StaffNode* q = p->next;
                p->next = q->next;
                delete q;
                cout << "删除成功!" << endl;
                saveStaff();
                return;
            }
            p = p->next;
        }
        cout << "未找到该员工。" << endl;
    }

    void assignJob() {
        string id;
        cout << "输入员工工号: "; cin >> id;
        StaffNode* p = staffHead->next;
        while(p) {
            if(p->id == id) {
                cout << "当前工作: " << p->job << endl;
                cout << "输入新工作分配: "; cin >> p->job;
                cout << "分配成功!" << endl;
                saveStaff();
                return;
            }
            p = p->next;
        }
        cout << "未找到该员工。" << endl;
    }

    void showAllStaff() {
        StaffNode* p = staffHead->next;
        if(!p) { cout << "暂无员工信息。" << endl; return; }
        cout << "\n--- 物业人员列表 ---" << endl;
        cout << left << setw(10) << "工号" << setw(10) << "姓名" << setw(5) << "年龄" 
             << setw(15) << "电话" << setw(10) << "级别" << setw(20) << "工作分配" << endl;
        while(p) {
            cout << left << setw(10) << p->id << setw(10) << p->name << setw(5) << p->age 
                 << setw(15) << p->phone << setw(10) << p->level << setw(20) << p->job << endl;
            p = p->next;
        }
    }

    // ================== 6. 菜单逻辑 ==================

    void menu() {
        while (true) {
            cout << "\n========================================" << endl;
            cout << "       小区物业管理系统       " << endl;
            cout << "========================================" << endl;
            cout << "1. 住户管理 (增删改查+统计)" << endl;
            cout << "2. 物业缴费 (子链表)" << endl;
            cout << "3. 报修服务 (队列+历史记录)" << endl;
            cout << "4. 社区活动 (树)" << endl;
            cout << "5. 物业人员管理 (链表)" << endl;
            cout << "6. 退出系统" << endl;
            cout << "请选择: ";
            
            int choice; cin >> choice;
            switch(choice) {
                case 1: {
                    cout << "[1.添加 2.修改 3.删除 4.查询 5.统计/列表]: ";
                    int sub; cin >> sub;
                    if(sub==1) {
                        string r, n, p; double a;
                        cout << "输入房号 姓名 电话 面积: ";
                        cin >> r >> n >> p >> a;
                        addResident(r, n, p, a);
                    } else if(sub==2) modifyResident();
                    else if(sub==3) deleteResident();
                    else if(sub==4) searchResident();
                    else if(sub==5) showAllResidents(); 
                    break;
                }
                case 2: {
                    cout << "[1.生成账单 2.住户缴费 3.查询历史]: ";
                    int sub; cin >> sub;
                    if(sub==1) addFee();
                    else if(sub==2) payFee();
                    else if(sub==3) showFeeHistory();
                    break;
                }
                case 3: {
                    cout << "[1.提交报修 2.分配任务 3.完成任务 4.住户评价 5.查看进度]: ";
                    int sub; cin >> sub;
                    if(sub==1) submitRepair();
                    else if(sub==2) processRepair();
                    else if(sub==3) finishRepair();
                    else if(sub==4) evaluateRepair();
                    else if(sub==5) showAllRepairs();
                    break;
                }
                case 4: {
                    cout << "[1.发布活动 2.查看活动树 3.住户报名 4.生成报告 5.删除活动]: ";
                    int sub; cin >> sub;
                    if(sub==1) addActivity();
                    else if(sub==2) showActivities();
                    else if(sub==3) joinActivity();
                    else if(sub==4) generateActivityReport();
                    else if(sub==5) deleteActivity();
                    break;
                }
                case 5: {
                    cout << "[1.添加员工 2.修改信息 3.删除员工 4.工作分配 5.员工列表]: ";
                    int sub; cin >> sub;
                    if(sub==1) addStaffMember();
                    else if(sub==2) modifyStaff();
                    else if(sub==3) deleteStaff();
                    else if(sub==4) assignJob();
                    else if(sub==5) showAllStaff();
                    break;
                }
                case 6: return;
                default: return;//cout << "输入无效！" << endl;
            }
        }
    }
};

int main() {
    // 设置 Windows 控制台代码页为 UTF-8，解决中文乱码问题
    #ifdef _WIN32
    system("chcp 65001");
    #endif

    PropertySystem system;
    system.menu();
    return 0;
}