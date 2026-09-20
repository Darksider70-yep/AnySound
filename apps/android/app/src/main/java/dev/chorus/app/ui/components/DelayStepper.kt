package dev.chorus.app.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.dp
import dev.chorus.app.ui.theme.ChorusTheme

@Composable
fun DelayStepper(
    offsetMs: Int,
    onOffsetChange: (Int) -> Unit,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(ChorusTheme.colors.deck)
            .border(1.dp, ChorusTheme.colors.line, RoundedCornerShape(12.dp))
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text(
            text = "Fine-tune delay",
            style = ChorusTheme.typography.bodyMedium,
            color = ChorusTheme.colors.mist
        )

        Spacer(modifier = Modifier.height(8.dp))

        Text(
            text = "${if (offsetMs > 0) "+" else ""}$offsetMs ms",
            style = ChorusTheme.typography.titleLarge,
            color = ChorusTheme.colors.fog
        )

        Spacer(modifier = Modifier.height(16.dp))

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceEvenly,
            verticalAlignment = Alignment.CenterVertically
        ) {
            StepButton("-10") { onOffsetChange((offsetMs - 10).coerceIn(-500, 500)) }
            StepButton("-1") { onOffsetChange((offsetMs - 1).coerceIn(-500, 500)) }
            StepButton("0") { onOffsetChange(0) }
            StepButton("+1") { onOffsetChange((offsetMs + 1).coerceIn(-500, 500)) }
            StepButton("+10") { onOffsetChange((offsetMs + 10).coerceIn(-500, 500)) }
        }
    }
}

@Composable
private fun StepButton(
    label: String,
    onClick: () -> Unit
) {
    Box(
        modifier = Modifier
            .size(width = 54.dp, height = 40.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(ChorusTheme.colors.line)
            .clickable { onClick() },
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = label,
            style = ChorusTheme.typography.bodyMedium,
            color = ChorusTheme.colors.fog
        )
    }
}
