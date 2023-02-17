// Project2.c : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//


#pragma comment(lib, "PowrProf.lib")

#if _DEBUG //debug時
    #pragma comment(lib, "jansson_d.lib")

#else      //release時
    #pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
    #pragma comment(lib, "jansson.lib")

#endif

#include <Windows.h>
#include <powersetting.h>
#include <stdio.h>
#include < combaseapi.h >
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <tchar.h>
#include <psapi.h>

#include <jansson.h>


#define POWER_SAVER       0
#define BALANCED          1
#define HIGH_PERFORMANCE 2
#define power_plan_num   3

# define MAX_NPROC 1024


typedef struct _power_plan{
    GUID guid;
    char name[100];
} POWER_PLAN;

const POWER_PLAN power_plan[power_plan_num] = {
    {{ 0xa1841308, 0x3541, 0x4fab, 0xbc, 0x81, 0xf7, 0x15, 0x56, 0xf2, 0x0b, 0x4a },"power_saver"},
    {{ 0x381b4222, 0xf694, 0x41f0, 0x96, 0x85, 0xff, 0x5b, 0xb2, 0x60, 0xdf, 0x2e },"balanced"},
    {{ 0x8c5e7fda,0xe8bf,0x4a96,0x9a,0x85,0xa6,0xe2,0x3a,0x8c,0x63,0x5c },"high_performance"}
};


int power_plan_name_from_GUID(GUID* guid,const char** power_plan_name)
{

    for (int i = 0; i < power_plan_num; i++) {
        if (IsEqualGUID(guid, &power_plan[i].guid) == TRUE)
        {
            *power_plan_name = power_plan[i].name;
            return EXIT_SUCCESS;
        }
    }
    return EXIT_FAILURE;
 
}

int get_active_power_plan_name(char** power_plan_name) {

    GUID* pPwrGUID;
    
    if (PowerGetActiveScheme(NULL, &pPwrGUID) != ERROR_SUCCESS) return -1;
    if (!power_plan_name_from_GUID(pPwrGUID, power_plan_name)) return -1;

    return EXIT_SUCCESS;
}

typedef struct APP_LIST {
    size_t count;
    char** names;
} app_list_s;

int new_app_list(app_list_s* list) {
    list->count = 0;
    list->names = (char**)malloc(sizeof(char*) * MAX_NPROC);
    if (list->names == NULL) { return EXIT_FAILURE; }
    for (int i = 0; i < MAX_NPROC; i++) {
        list->names[i] = (char*)malloc(sizeof(char) * MAX_PATH);
        if (list->names[i] == NULL) { return EXIT_FAILURE; }
    }
    return EXIT_SUCCESS;
}

int del_app_list(app_list_s* list) {
    for (int i = 0; i < MAX_NPROC; i++) {
        free(list->names[i]);
    }
    free(list->names);
    list->names = NULL;
    return EXIT_SUCCESS;
}

int get_powerplan_app_map(app_list_s* powerplan_app_map) {
    json_t* json_root;
    json_error_t jerror;

    json_root = json_load_file("./app_list.json", 0, &jerror);

    if (json_root == NULL) {
        fputs(jerror.text, stderr);
        return -1;
    }


    for (int p = 0; p < power_plan_num; ++p) {
        powerplan_app_map[p].count = 0;
        json_t* name_array = json_object_get(json_root, power_plan[p].name);
        if (!json_is_array(name_array)) { continue; }


        size_t name_array_size = json_array_size(name_array);
        if (name_array_size == 0) { continue; }

        powerplan_app_map[p].names = (char**)malloc(sizeof(char*) * name_array_size);
        if (powerplan_app_map[p].names == NULL) { json_decref(json_root); return -1; }

        for (int i = 0; i < name_array_size; ++i) {
            powerplan_app_map[p].names[i] = (char*)malloc(sizeof(char) * (MAX_PATH + 1));
            if (powerplan_app_map[p].names[i] == NULL) {
                json_decref(json_root); return -1;
            }

            json_t* j_name = json_array_get(name_array, i);
            if (!json_is_string(j_name)) {
                printf("bad json file\n");
                json_decref(json_root); return -1;
            }
            strncpy_s(powerplan_app_map[p].names[i], MAX_PATH, json_string_value(j_name), MAX_PATH);
            // _tprintf(TEXT("%s\n"), w_name);
        }
        powerplan_app_map[p].count = name_array_size;
        

    }
    
    json_decref(json_root);

    return 0;



}



int get_process_list(app_list_s* process_list) {


    if (process_list->names == NULL) { return -1; }

    DWORD cbNeeded, nProc;

    size_t size_allProc = MAX_NPROC * sizeof(DWORD);
    DWORD* allProc = NULL;
    allProc = (DWORD *)malloc(size_allProc);
    process_list->count = 0;

    if (allProc == NULL) {
        printf("メモリ不足！");
        return -1;
    }

        
    if (!EnumProcesses(allProc, (DWORD)size_allProc, &cbNeeded)) {
        return -1;
    }

    nProc = cbNeeded / sizeof(DWORD);


    for (int i = 0; i < nProc; i++) {
        TCHAR procName[MAX_PATH] = TEXT("<unknown>");

        HANDLE hProcess = OpenProcess(
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
            FALSE, allProc[i]);
        if (NULL != hProcess) {
            HMODULE hMod;
            DWORD cbNeeded;

            if (EnumProcessModulesEx(hProcess, &hMod, sizeof(hMod), &cbNeeded, LIST_MODULES_ALL)) {
                GetModuleBaseName(hProcess, hMod, procName,
                    sizeof(procName) / sizeof(TCHAR));
            }
            CloseHandle(hProcess);
        }
        size_t rv;
        wcstombs_s(
            &rv,
            process_list->names[i],
            MAX_PATH,
            procName,
            _TRUNCATE
        );
       
    }
    process_list->count = nProc;
    free(allProc);
    allProc = NULL;


}







int main() {
    // json ファイル
    app_list_s powerplan_app_map[power_plan_num];
    get_powerplan_app_map(powerplan_app_map);


    app_list_s proc_list;

    bool should_enable_power_plan[power_plan_num];

    if(EXIT_SUCCESS != new_app_list(&proc_list)) return -1;
    
    while (TRUE) {
        get_process_list(&proc_list);
    
        for (int p = 0; p < power_plan_num; ++p) {
            should_enable_power_plan[p] = false;
            for (int i = 0; i < (int)proc_list.count && !should_enable_power_plan[p]; ++i) {
                for (int j = 0; j < (int)powerplan_app_map[p].count; ++j) {
                    if (strstr(proc_list.names[i], powerplan_app_map[p].names[j]) != NULL) {
                        should_enable_power_plan[p] = true;
                        break;
                    }
                }
            }
        }

        if(should_enable_power_plan[HIGH_PERFORMANCE])
            PowerSetActiveScheme(NULL, &power_plan[HIGH_PERFORMANCE].guid);
        else if (should_enable_power_plan[BALANCED])
            PowerSetActiveScheme(NULL, &power_plan[BALANCED].guid);
        else 
            PowerSetActiveScheme(NULL, &power_plan[POWER_SAVER].guid);

        #if _DEBUG
            printf("%d %d %d\n", should_enable_power_plan[HIGH_PERFORMANCE], should_enable_power_plan[BALANCED], should_enable_power_plan[POWER_SAVER]);
        #endif
        Sleep(2000);
    }

    

    return 0;

}




// プログラムの実行: Ctrl + F5 または [デバッグ] > [デバッグなしで開始] メニュー
// プログラムのデバッグ: F5 または [デバッグ] > [デバッグの開始] メニュー

// 作業を開始するためのヒント: 
//    1. ソリューション エクスプローラー ウィンドウを使用してファイルを追加/管理します 
//   2. チーム エクスプローラー ウィンドウを使用してソース管理に接続します
//   3. 出力ウィンドウを使用して、ビルド出力とその他のメッセージを表示します
//   4. エラー一覧ウィンドウを使用してエラーを表示します
//   5. [プロジェクト] > [新しい項目の追加] と移動して新しいコード ファイルを作成するか、[プロジェクト] > [既存の項目の追加] と移動して既存のコード ファイルをプロジェクトに追加します
//   6. 後ほどこのプロジェクトを再び開く場合、[ファイル] > [開く] > [プロジェクト] と移動して .sln ファイルを選択します

//int main()
//{
//    //GUID* pPwrGUID;
//    //PowerGetActiveScheme(NULL, &pPwrGUID);
//
//    //LPOLESTR guid_str = NULL;
//
//    //if (StringFromCLSID(pPwrGUID, &guid_str) == S_OK) {
//    //    printf("%ls\n", guid_str);
//    //}
//    //else {
//    //    printf("%s\n", "エラー！");
//    //}
//
//    char* plan_name;
//    
//
//    //if (power_plan_name_from_GUID(pPwrGUID, &plan_name) == EXIT_SUCCESS) 
//    //{
//    //    printf("%s\n",plan_name);
//    //}
//    //else {
//    //    printf("%s\n", "エラー！");
//    //}
//
//    get_active_power_plan_name(&plan_name);
//    printf("%s\n", plan_name);
//
//    while (TRUE) {
//
//        if (!process_exist()) {
//            PowerSetActiveScheme(NULL, &power_plan[HIGH_PERFORMANCE].guid);
//            printf("%s\n", power_plan[HIGH_PERFORMANCE].name);
//        }
//        else {
//            PowerSetActiveScheme(NULL, &power_plan[POWER_SAVE].guid);
//            printf("%s\n", power_plan[POWER_SAVE].name);
//        }
//
//        Sleep(1000);
//    }
//
//
//    system("pause");
//}