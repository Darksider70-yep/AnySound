package dev.chorus.app.ui.components

import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.unit.dp
import dev.chorus.app.ui.theme.ChorusTheme
import kotlin.math.max
import kotlin.math.min

@Composable
fun SonarRingView(
    syncErrorMs: Float,
    isActive: Boolean,
    modifier: Modifier = Modifier.size(240.dp)
) {
    val infiniteTransition = rememberInfiniteTransition(label = "sonar_pulse")
    val pulseProgress by infiniteTransition.animateFloat(
        initialValue = 0.0f,
        targetValue = 1.0f,
        animationSpec = infiniteRepeatable(
            animation = tween(durationMillis = 2000, easing = LinearEasing),
            repeatMode = RepeatMode.Restart
        ),
        label = "sonar_pulse_progress"
    )

    val sonarColor = ChorusTheme.colors.sonar
    val driftColor = ChorusTheme.colors.drift
    val lostColor = ChorusTheme.colors.lost

    val ringColor = when {
        !isActive -> lostColor
        kotlin.math.abs(syncErrorMs) <= 5.0f -> sonarColor
        kotlin.math.abs(syncErrorMs) <= 20.0f -> driftColor
        else -> lostColor
    }

    val phaseOffset = if (isActive) {
        val clamped = max(-1.0f, min(1.0f, syncErrorMs / 20.0f))
        clamped * 0.5f
    } else 0.0f

    Canvas(modifier = modifier) {
        val center = Offset(size.width / 2.0f, size.height / 2.0f)
        val maxRadius = min(size.width, size.height) / 2.0f - 16.dp.toPx()

        if (!isActive) {
            // Static single line when inactive
            drawCircle(
                color = ringColor.copy(alpha = 0.3f),
                radius = maxRadius * 0.4f,
                center = center,
                style = Stroke(width = 2.dp.toPx())
            )
            drawCircle(
                color = ringColor.copy(alpha = 0.6f),
                radius = 6.dp.toPx(),
                center = center
            )
            return@Canvas
        }

        // Draw 3 animated expanding sonar ripples
        val numRings = 3
        for (i in 0 until numRings) {
            val ringFraction = (pulseProgress + i.toFloat() / numRings.toFloat()) % 1.0f
            val currentRadius = ringFraction * maxRadius
            val alpha = (1.0f - ringFraction).coerceIn(0.0f, 1.0f) * 0.5f

            // Phase shifted center for non-zero sync error
            val displacedCenter = Offset(
                x = center.x + phaseOffset * 20.dp.toPx() * (1.0f - ringFraction),
                y = center.y
            )

            drawCircle(
                color = ringColor.copy(alpha = alpha),
                radius = currentRadius,
                center = displacedCenter,
                style = Stroke(width = (2.5f * (1.0f - ringFraction * 0.5f)).dp.toPx())
            )
        }

        // Solid inner core
        drawCircle(
            color = ringColor.copy(alpha = 0.2f),
            radius = 24.dp.toPx(),
            center = center
        )
        drawCircle(
            color = ringColor,
            radius = 8.dp.toPx(),
            center = center
        )
    }
}
