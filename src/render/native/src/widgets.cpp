#include "render_native/widgets.hpp"

namespace aura::render_native {

std::string widget_backend_name() {
#if defined(RENDER_NATIVE_QT_WIDGETS)
    return "qt_widgets_rhi";
#else
    return "qt_widgets_rhi_stub";
#endif
}

bool widget_backend_available() {
#if defined(RENDER_NATIVE_QT_WIDGETS)
    return true;
#else
    return false;
#endif
}

}  // namespace aura::render_native
