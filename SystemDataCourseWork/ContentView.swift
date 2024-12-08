//
//  ContentView.swift
//  SystemDataCourseWork
//
//  Created by Александр Плешаков on 08.12.2024.
//

import SwiftUI

struct ContentView: View {
    @StateObject var viewModel = ContentViewModel()
    @StateObject var networkClient = NetworkClient()
    
    var body: some View {
        VStack {
            Text(networkClient.durationOfWork ?? "None duration Of Work")
            Text(networkClient.timeZone ?? "None time zone")
            Text(networkClient.lastReceiveFromTimeServer ?? "None time connection")
            HStack {
                Button("Connect to Server") {
                    networkClient.connectTimeServer()
                }
                Button("Disconnect") {
                    networkClient.disconnectTimeServer()
                }
            }
        }
        .padding()
//        .onAppear {
//            networkClient.connectTimeServer()
//        }
//        .onDisappear {
//            networkClient.disconnectTimeServer()
//        }
    }
}

#Preview {
    ContentView()
}
