#include <QApplication> // Qt application and event loop
#include <QWidget>      // Base class for all GUI windows
#include <QSlider>      // Slider widget for brightness control
#include <QLabel>       // Labels for LED names
#include <QPushButton>  // Exit button
#include <QVBoxLayout>  // Vertical layout manager
#include <QHBoxLayout>  // Horizontal layout for labels and sliders
#include <QFont>        // Font customization
#include <QPalette>     // GUI background color
#include <QTimer>       // Timer for SIT730 automatic intensity modulation
#include <memory>       // std::unique_ptr and std::shared_ptr for smart memory management
#include <stdexcept>    // For throwing runtime errors
#include <pigpio.h>     // Raspberry Pi GPIO control (PWM)

// GPIO pin numbers connected to respective LEDs
constexpr int RED_LED{17};
constexpr int GREEN_LED{27};
constexpr int BLUE_LED{22};

/**
 * Initializes pigpio and sets GPIO pins as output.
 */
void setupGpio()
{
    if (gpioInitialise() < 0)
    {
        throw std::runtime_error{"GPIO initialization failed"};
    }
    gpioSetMode(RED_LED, PI_OUTPUT);
    gpioSetMode(GREEN_LED, PI_OUTPUT);
    gpioSetMode(BLUE_LED, PI_OUTPUT);
}

/**
 * Creates a single LED slider widget used for PWM brightness control.
 * This version is used only for the Red LED (manual control).
 */
std::unique_ptr<QWidget> createLedSlider(const QString &labelText, int gpioPin)
{
    auto label{std::make_unique<QLabel>(labelText)};
    label->setFont(QFont{"Arial", 11});
    label->setStyleSheet("QLabel { color: white; }");

    auto slider{std::make_unique<QSlider>(Qt::Horizontal)};
    slider->setRange(0, 255); // Range for PWM (duty cycle)
    slider->setValue(0);      // Default off

    // Connect slider movement to update the LED brightness using PWM
    QObject::connect(slider.get(), &QSlider::valueChanged, [gpioPin](int value)
                     { gpioPWM(gpioPin, value); });

    auto layout{std::make_unique<QHBoxLayout>()};
    layout->addWidget(label.get());
    layout->addWidget(slider.get());

    auto container{std::make_unique<QWidget>()};
    container->setLayout(layout.release());

    label.release();
    slider.release();

    return container;
}

/**
 * Creates an Exit button that shuts down the GUI application cleanly.
 */
std::unique_ptr<QPushButton> createExitButton()
{
    auto button{std::make_unique<QPushButton>("Exit")};
    button->setFont(QFont{"Arial", 12});
    button->setStyleSheet("QPushButton { background-color: grey; color: white; padding: 5px; }");

    QObject::connect(button.get(), &QPushButton::clicked, []()
                     { QApplication::quit(); });

    return button;
}

/**
 * Creates a QTimer that automatically adjusts the brightness of
 * GREEN and BLUE LEDs to create a continuous fading effect.
 * GREEN and BLUE will have opposing brightness patterns.
 */
void setupAutoIntensityTimer(const std::shared_ptr<QWidget> &parent)
{
    // This struct holds the current brightness level and direction
    // and is shared across timer executions using a shared_ptr.
    struct TimerState
    {
        int brightness{0};     // PWM value for GREEN LED (0–255)
        bool increasing{true}; // Flag to track whether we're fading up or down
    };

    // Shared state object between QTimer and lambda — needed so brightness
    // can be updated and remembered across timeouts.
    auto state{std::make_shared<TimerState>()};

    // Create the timer with the given parent (which manages its memory).
    // The parent ensures the timer is properly cleaned up when GUI closes.
    auto timer{std::make_unique<QTimer>(parent.get())};

    // Every 20ms, this lambda runs to update LED brightness via PWM.
    QObject::connect(timer.get(), &QTimer::timeout, [state]()
                     {
        // Set brightness of GREEN LED directly, and BLUE LED inversely
        // This gives a "see-saw" brightness effect between the two LEDs.
        gpioPWM(GREEN_LED, state->brightness);
        gpioPWM(BLUE_LED, 255 - state->brightness);

        // Adjust brightness smoothly based on direction.
        constexpr int step{2};  // Smaller step = smoother fade animation

        if (state->increasing) {
            state->brightness += step;
            if (state->brightness >= 255) {
                state->brightness = 255;         // Cap max value
                state->increasing = false;       // Start fading down
            }
        } else {
            state->brightness -= step;
            if (state->brightness <= 0) {
                state->brightness = 0;           // Cap min value
                state->increasing = true;        // Start fading up
            }
        } });

    // Start the timer: this will call the lambda every 20ms
    timer->start(20);

    // We release ownership because the parent (main window) now owns the timer
    timer.release(); // Prevents double deletion
}

/**
 * Builds the complete GUI:
 * - Red LED is manually controlled with a slider.
 * - Green and Blue LEDs are controlled by the automated timer.
 */
std::unique_ptr<QWidget> createGui()
{
    auto window{std::make_unique<QWidget>()};
    window->setWindowTitle("PWM LED Brightness Controller");
    window->setFixedSize(440, 200);

    // Set dark theme background
    QPalette palette{window->palette()};
    palette.setColor(QPalette::Window, Qt::black);
    window->setAutoFillBackground(true);
    window->setPalette(palette);

    // Only red LED has manual control
    auto redSlider{createLedSlider("Red LED", RED_LED)};
    auto exitButton{createExitButton()};
    auto layout{std::make_unique<QVBoxLayout>()};

    layout->addWidget(redSlider.release()); // Red LED slider widget
    layout->addWidget(exitButton.get());    // Exit button
    layout->setAlignment(exitButton.get(), Qt::AlignCenter);
    layout->addStretch();

    exitButton.release();
    window->setLayout(layout.release());

    // Set up automated PWM modulation for Green and Blue LEDs only
    std::shared_ptr<QWidget> sharedWindow(window.get(), [](QWidget *) {});
    setupAutoIntensityTimer(sharedWindow);

    return window;
}

/**
 * Main entry point. Initializes GPIO and starts the Qt GUI loop.
 */
int main(int argc, char *argv[])
{
    QApplication app{argc, argv};

    try
    {
        setupGpio();

        // Ensure LEDs are safely turned off on application exit
        QObject::connect(&app, &QCoreApplication::aboutToQuit, []()
                         {
            gpioPWM(RED_LED, 0);
            gpioPWM(GREEN_LED, 0);
            gpioPWM(BLUE_LED, 0);
            gpioTerminate(); });

        auto window{createGui()};
        window->show();

        return app.exec();
    }
    catch (const std::exception &ex)
    {
        qCritical("Startup Error: %s", ex.what());
        gpioTerminate();
        return 1;
    }
}