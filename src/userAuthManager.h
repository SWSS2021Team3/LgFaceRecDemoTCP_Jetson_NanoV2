
#include <string>

class UserAuthManager
{
public:
    UserAuthManager() {}
    bool verifyUser(std::string userid, std::string passwd);
private:
};
