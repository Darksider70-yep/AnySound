#ifdef QT_CORE_LIB
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include "ui_controller.hpp"

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setOrganizationName("Chorus");
    app.setApplicationName("Chorus Desktop");

    QQmlApplicationEngine engine;

    chorus::ui::UiController uiController;
    engine.rootContext()->setContextProperty("uiController", &uiController);

    const QUrl url(QStringLiteral("qrc:/chorus/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject* obj, const QUrl& objUrl) {
        if (!obj && url == objUrl) {
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
#else
#include <iostream>

int main() noexcept {
    try {
        std::cout << "Chorus Desktop requires Qt 6 (Core, Gui, Qml, Quick, QuickControls2).\n";
    } catch (...) {
        return 1;
    }
    return 0;
}
#endif
