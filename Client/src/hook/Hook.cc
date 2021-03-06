//
// Created by kiosk on 2020/1/31.
//
#include "Hook.h"

Hook::Hook(Epoll& epoll) : m_epoll(epoll), m_poll(std::make_unique<thread_pool>()) {
}

Hook::~Hook() {
    for (auto& c : m_table) {
        close(c);
    }
};

bool Hook::Init(std::string&& FilePath) {
    try {
        auto& temp = FilePath;
        int m_fd = fanotify_init(FAN_CLASS_CONTENT, O_RDWR);
        auto ret =
            fanotify_mark(m_fd, FAN_MARK_ADD, FAN_CLOSE | FAN_OPEN_PERM | FAN_EVENT_ON_CHILD, AT_FDCWD, temp.c_str());
        m_table.emplace_back(m_fd);
        m_epoll.Add_Channel(m_fd, Readable());
        DIR* m_dir = nullptr;            // 文件目录指针
        struct dirent* m_ptr = nullptr;  // 文件目录结构体
        m_dir = opendir(temp.c_str());
        while ((m_ptr = readdir(m_dir)) != nullptr) {
            if (m_ptr->d_type == 4) {
                if (!strncasecmp(m_ptr->d_name, ".", 1) || !strncasecmp(m_ptr->d_name, "..", 2)
                    || !strncasecmp(m_ptr->d_name, ".git", 4)) {
                    continue;
                } else {
                    FilePath = temp + '/' + m_ptr->d_name;
                    Init(std::move(FilePath));
                }
            } else {
                continue;
            }
        }
    } catch (std::bad_exception& c) {
        puts("fanotify_init Error");
        return false;
    }
    return true;
}

int Hook::AlloworDisAllow(int fd, int flag, int filefd) {
    try {
        struct fanotify_response response {};
        response.fd = filefd;
        response.response = flag;  // allow or disallow
        if (write(fd, &response, sizeof(struct fanotify_response)) < 0) {
            throw std::invalid_argument(strerror(errno));
        }
    } catch (std::invalid_argument& e) {
        printf("%s\n", e.what());
    }
}

void Hook::RunOnce(int fd) {
    char buffer[65535];
    ssize_t len = 0;
    if ((len = read(fd, buffer, sizeof(buffer))) > 0) {
        auto metadata = reinterpret_cast<struct fanotify_event_metadata*>(&buffer);
        while (FAN_EVENT_OK(metadata, len)) {
            auto name = File::ReadPath(metadata->fd);
            if (!name.empty() && name != ".gitignore") {
                //获取相应事件
                if (metadata->mask & FAN_Q_OVERFLOW) {
                    continue;
                }
                if (!m_callback()) {
                    AlloworDisAllow(fd, FAN_DENY, metadata->fd);
                    close(metadata->fd);
                    break;
                }  //说明状态没有连接
                auto&& file_handle = Getcount(name);
                if (metadata->mask & FAN_OPEN_PERM) {
                    file_handle->m_count++;  // 引用计数++
                    if (file_handle->m_count > 1) {
                        //说明不止一个进程打开了文件
                        AlloworDisAllow(fd, FAN_ALLOW, metadata->fd);
                        continue;
                    }
                    file_handle->m_Backup = true;  //开始备份
                    //获取文件内容
                    file_handle->ReadySend(metadata->fd, m_poll, name);
                    AlloworDisAllow(fd, FAN_ALLOW, metadata->fd);
                } else if ((metadata->mask & FAN_CLOSE_WRITE) || (metadata->mask & FAN_CLOSE_NOWRITE)) {
                    file_handle->m_count--;
                    if (file_handle->m_count == 0) {
                        //恢复
                        file_handle->SendClose(m_serverfd(), name);
                        file_handle->m_Backup = false;
                    }
                }
            }
            close(metadata->fd);
            metadata = FAN_EVENT_NEXT(metadata, len);
        }
    }
}

auto Hook::Getcount(const std::string& namefile) -> decltype(m_count[namefile]) {
    if (m_count.find(namefile) == m_count.end()) {
        m_count.emplace(namefile, std::make_unique<File>());
    }
    return m_count[namefile];
}
