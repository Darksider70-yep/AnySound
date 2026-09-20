package dev.chorus.app.ui.theme

import android.app.Activity
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.Immutable
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalView
import androidx.core.view.WindowCompat

@Immutable
data class ChorusColors(
    val harbor: Color,
    val deck: Color,
    val line: Color,
    val fog: Color,
    val mist: Color,
    val sonar: Color,
    val drift: Color,
    val lost: Color
)

val LocalChorusColors = staticCompositionLocalOf {
    ChorusColors(
        harbor = DarkHarbor,
        deck = DarkDeck,
        line = DarkLine,
        fog = DarkFog,
        mist = DarkMist,
        sonar = DarkSonar,
        drift = DarkDrift,
        lost = DarkLost
    )
}

object ChorusTheme {
    val colors: ChorusColors
        @Composable
        get() = LocalChorusColors.current
    val typography: androidx.compose.material3.Typography
        @Composable
        get() = MaterialTheme.typography
}

private val DarkColorScheme = darkColorScheme(
    primary = DarkSonar,
    onPrimary = DarkHarbor,
    background = DarkHarbor,
    onBackground = DarkFog,
    surface = DarkDeck,
    onSurface = DarkFog,
    outline = DarkLine
)

private val LightColorScheme = lightColorScheme(
    primary = LightSonar,
    onPrimary = LightDeck,
    background = LightHarbor,
    onBackground = LightFog,
    surface = LightDeck,
    onSurface = LightFog,
    outline = LightLine
)

@Composable
fun ChorusAppTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit
) {
    val chorusColors = if (darkTheme) {
        ChorusColors(
            harbor = DarkHarbor,
            deck = DarkDeck,
            line = DarkLine,
            fog = DarkFog,
            mist = DarkMist,
            sonar = DarkSonar,
            drift = DarkDrift,
            lost = DarkLost
        )
    } else {
        ChorusColors(
            harbor = LightHarbor,
            deck = LightDeck,
            line = LightLine,
            fog = LightFog,
            mist = LightMist,
            sonar = LightSonar,
            drift = LightDrift,
            lost = LightLost
        )
    }

    val colorScheme = if (darkTheme) DarkColorScheme else LightColorScheme

    val view = LocalView.current
    if (!view.isInEditMode) {
        SideEffect {
            val window = (view.context as Activity).window
            window.statusBarColor = chorusColors.harbor.toArgb()
            window.navigationBarColor = chorusColors.harbor.toArgb()
            WindowCompat.getInsetsController(window, view).apply {
                isAppearanceLightStatusBars = !darkTheme
                isAppearanceLightNavigationBars = !darkTheme
            }
        }
    }

    CompositionLocalProvider(LocalChorusColors provides chorusColors) {
        MaterialTheme(
            colorScheme = colorScheme,
            typography = Typography,
            content = content
        )
    }
}
