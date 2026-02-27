package com.BLQ.AnonProject

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.BLQ.AnonProject.ui.theme.粉色奶龙Theme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.PrintWriter
import java.net.Socket

data class PetState(
    val online: Boolean = false,
    val satiety: Int = 0,
    val mood: Int = 50,
    val statusText: String = "未连接"
)

private const val SERVER_HOST = "frp-bus.com"
private const val SERVER_PORT = 47169

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent { 粉色奶龙Theme { PetScreen() } }
    }
}

@Composable
private fun PetScreen() {
    var state by remember { mutableStateOf(PetState()) }

    LaunchedEffect(Unit) {
        while (true) {
            state = fetchState() ?: state
            delay(3000)
        }
    }

    val topAlert = if (state.satiety < 20) "要饿死了" else ""
    val imageRes = when {
        state.mood == 0 -> R.drawable.angry
        state.mood < 20 -> R.drawable.sad
        else -> R.drawable.happy
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xFFFDF4FF))
            .padding(24.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        if (topAlert.isNotEmpty()) {
            Text(text = topAlert, color = Color.Red, fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.height(8.dp))
        }
        Text(text = state.statusText, style = MaterialTheme.typography.titleMedium)
        Spacer(modifier = Modifier.height(12.dp))
        Image(
            painter = painterResource(imageRes),
            contentDescription = "pet mood",
            modifier = Modifier.size(220.dp),
            contentScale = ContentScale.Crop
        )
        Spacer(modifier = Modifier.height(24.dp))
        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
            Text(text = "饱食度: ${state.satiety}%")
            Text(text = "心情值: ${state.mood}")
        }
    }
}

private suspend fun fetchState(): PetState? = withContext(Dispatchers.IO) {
    runCatching {
        Socket(SERVER_HOST, SERVER_PORT).use { socket ->
            socket.soTimeout = 3500
            val writer = PrintWriter(socket.getOutputStream(), true)
            val reader = BufferedReader(InputStreamReader(socket.getInputStream()))
            writer.println("{\"type\":\"app_get\"}")
            val resp = reader.readLine() ?: return@use PetState(statusText = "服务器无响应")
            val j = JSONObject(resp)
            PetState(
                online = j.optBoolean("online", false),
                satiety = j.optInt("satiety", 0),
                mood = j.optInt("mood", 50),
                statusText = j.optString("statusText", "正常")
            )
        }
    }.getOrElse { PetState(statusText = "连接失败") }
}
