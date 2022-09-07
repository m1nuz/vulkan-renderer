#include "Application.hpp"
#include "Journal.hpp"
#include "Tags.hpp"

extern int main(int, char**) {
    Application::Configuration conf;
    Application::Application app;
    return Application::run(conf, app);
}
