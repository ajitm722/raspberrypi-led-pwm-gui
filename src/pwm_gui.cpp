#include <QApplication>     // Qt application and event loop
#include <QWidget>          // Base class for all GUI windows
#include <QSlider>          // Slider widget for brightness control
#include <QLabel>           // Labels for LED names
#include <QPushButton>      // Exit button
#include <QVBoxLayout>      // Vertical layout manager
#include <QHBoxLayout>      // Horizontal layout for labels and sliders
#include <QFont>            // Font customization
#include <QPalette>         // GUI background color
#include <memory>           // std::unique_ptr for memory safety
#include <stdexcept>        // For throwing runtime errors
#include <pigpio.h>         // Raspberry Pi GPIO control (PWM)

// GPIO pin assignments
constexpr int RED_LED{17};
constexpr int GREEN_LED{27};
constexpr int BLUE_LED{22};

/**
 * Initializes pigpio and sets up the LED pins for PWM output.
 */
void setupGpio() {
    if (gpioInitialise() < 0) {
        throw std::runtime_error{"GPIO initialization failed"};
    }

    // Set each pin as output (required for pwm)
    gpioSetMode(RED_LED, PI_OUTPUT);
    gpioSetMode(GREEN_LED, PI_OUTPUT);
    gpioSetMode(BLUE_LED, PI_OUTPUT);
}

/**
 * Creates a labeled slider that adjusts brightness via PWM on a given GPIO pin.
 */
std::unique_ptr<QWidget> createLedSlider(const QString& labelText, int gpioPin) {
    auto label{std::make_unique<QLabel>(labelText)};
    label->setFont(QFont{"Arial", 11});
    label->setStyleSheet("QLabel { color: white; }\n");

    auto slider{std::make_unique<QSlider>(Qt::Horizontal)};
    slider->setRange(0, 255); // PWM range
    slider->setValue(0);

    QObject::connect(slider.get(), &QSlider::valueChanged, [gpioPin](int value) {
        gpioPWM(gpioPin, value);
    });

    auto layout{std::make_unique<QHBoxLayout>()};
    layout->addWidget(label.get());
    layout->addWidget(slider.get());

    auto container{std::make_unique<QWidget>()};
    container->setLayout(layout.release());

    // Prevent double deletion
    label.release();
    slider.release();

    return container;
}

/**
 * Creates the Exit button to close the app and trigger GPIO cleanup.
 */
std::unique_ptr<QPushButton> createExitButton() {
    auto button{std::make_unique<QPushButton>("Exit")};
    button->setFont(QFont{"Arial", 12});
    button->setStyleSheet("QPushButton { background-color: grey; color: white; padding: 5px; }");

    QObject::connect(button.get(), &QPushButton::clicked, []() {
        QApplication::quit();
    });

    return button;
}

/**
 * Constructs and returns the full GUI.
 */
std::unique_ptr<QWidget> createGui() {
    auto window{std::make_unique<QWidget>()};
    window->setWindowTitle("PWM LED Brightness Controller");
    window->setFixedSize(440, 250);

    QPalette palette{window->palette()};
    palette.setColor(QPalette::Window, Qt::black);
    window->setAutoFillBackground(true);
    window->setPalette(palette);

    // Create components
    auto redSlider{createLedSlider("Red LED", RED_LED)};
    auto greenSlider{createLedSlider("Green LED", GREEN_LED)};
    auto blueSlider{createLedSlider("Blue LED", BLUE_LED)};
    auto exitButton{createExitButton()};
    auto layout{std::make_unique<QVBoxLayout>()};

    // Add components to layout
    layout->addWidget(redSlider.release());
    layout->addWidget(greenSlider.release());
    layout->addWidget(blueSlider.release());
    layout->addWidget(exitButton.get());
    layout->setAlignment(exitButton.get(), Qt::AlignCenter);
    layout->addStretch();

    exitButton.release();
    window->setLayout(layout.release());
    return window;
}

/**
 * Main function: initializes GPIO, sets cleanup, and runs Qt GUI.
 */
int main(int argc, char* argv[]) {
    QApplication app{argc, argv};

    try {
        setupGpio();

        QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
            gpioPWM(RED_LED, 0);
            gpioPWM(GREEN_LED, 0);
            gpioPWM(BLUE_LED, 0);
            gpioTerminate();
        });

        auto window{createGui()};
        window->show();

        return app.exec();
    }
    catch (const std::exception& ex) {
        qCritical("Startup Error: %s", ex.what());
        gpioTerminate();
        return 1;
    }
}

