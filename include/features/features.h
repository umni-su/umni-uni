#pragma once

#ifndef UMNI_DEVICE_NAME
#define UMNI_DEVICE_NAME "Unknown"
#endif

#ifndef UMNI_DEVICE_MODEL
#define UMNI_DEVICE_MODEL "Unknown"
#endif

// ============================================
// МАКРОСЫ ДЛЯ РАБОТЫ С ФИЧАМИ
// ============================================

// Проверка включения фичи
#define UM_FEATURE_ENABLED(name) (UM_FEATURE_##name == 1)
#define UM_FEATURE_DISABLED(name) (UM_FEATURE_##name == 0)

// Условное выполнение кода
#define IF_FEATURE_ENABLED(name, code) if (UM_FEATURE_ENABLED(name)) { code }
#define IF_FEATURE_DISABLED(name, code) if (UM_FEATURE_DISABLED(name)) { code }
