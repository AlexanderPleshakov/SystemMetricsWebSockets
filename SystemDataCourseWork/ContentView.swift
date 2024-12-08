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
    @State var timeServerConnected: Bool = false
    @State var systemServerConnected: Bool = false
    
    var body: some View {
        HStack {
            ZStack {
                Color(.white)
                    .background(timeServerConnected ? .green : .red).opacity(0.08)
                    .clipShape(RoundedRectangle(cornerRadius: 20), style: FillStyle())
                VStack {
                    Text("Сервер 1")
                        .font(.system(size: 40, weight: .semibold))
                        .padding()
                    
                    
                    VStack(alignment: .center, spacing: 10) {
                        Text(networkClient.lastReceiveFromTimeServer == nil ? "" : "Последнее подключение:")
                            .font(.system(size: 16))
                        Text(networkClient.lastReceiveFromTimeServer ?? "")
                            .font(.system(size: 12))
                            .padding()
                            .background(networkClient.lastReceiveFromTimeServer == nil ? .clear : .black.opacity(0.1))
                            .clipShape(RoundedRectangle(cornerRadius: 16), style: FillStyle())
                    }
                    .padding(.top, 20)
                    .padding(.bottom, 10)
                    .frame(maxWidth: .infinity)
                    .background(.black.opacity(0.1))
                    .clipShape(RoundedRectangle(cornerRadius: 16), style: FillStyle())
                    
                    Spacer()
                    
                    VStack(alignment: .center) {
                        Spacer()
                        
                        VStack {
                            Text(networkClient.timeZone != nil ? "Часовой пояс:" : "")
                                .font(.system(size: 16))
                                .padding(.bottom, 2)
                            Text(networkClient.timeZone ?? "")
                                .font(.system(size: 40, weight: .bold))
                                .italic()
                                .foregroundStyle(.cyan)
                        }
                        
                        Spacer()
                        
                        VStack {
                            Text(networkClient.durationOfWork != nil ? "Продолжительность:" : "")
                                .font(.system(size: 16))
                                .padding(.bottom, 2)
                            Text(networkClient.durationOfWork ?? "")
                                .font(.system(size: 40))
                                .italic()
                                .foregroundStyle(.cyan)
                        }
                        
                        Spacer()
                        
                    }
                    
                    Spacer()
                    
                    
                    HStack() {
                        Button(timeServerConnected ? "Отключиться" : "Подключиться") {
                            if timeServerConnected {
                                networkClient.disconnectTimeServer()
                            } else {
                                networkClient.connectTimeServer()
                            }
                            withAnimation {
                                timeServerConnected.toggle()
                            }
                        }
                        .buttonStyle(
                            GrowingButton(
                                color: timeServerConnected ? .red : .green.opacity(0.7)
                            )
                        )
                    }
                    .padding(.horizontal, 16)
                }
                .padding()
            }
            .padding(30)
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            
            // Second server
            
            ZStack {
                Color(.white)
                    .background(systemServerConnected ? .green : .red).opacity(0.08)
                    .clipShape(RoundedRectangle(cornerRadius: 20), style: FillStyle())
                VStack {
                    Text("Сервер 2")
                        .font(.system(size: 40, weight: .semibold))
                        .padding()
                    
                    
                    VStack(alignment: .center, spacing: 10) {
                        Text(networkClient.lastReceiveFromTimeServer == nil ? "" : "Последнее подключение:")
                            .font(.system(size: 16))
                        Text(networkClient.lastReceiveFromTimeServer ?? "")
                            .font(.system(size: 12))
                            .padding()
                            .background(networkClient.lastReceiveFromTimeServer == nil ? .clear : .black.opacity(0.1))
                            .clipShape(RoundedRectangle(cornerRadius: 16), style: FillStyle())
                    }
                    .padding(.top, 20)
                    .padding(.bottom, 10)
                    .frame(maxWidth: .infinity)
                    .background(.black.opacity(0.1))
                    .clipShape(RoundedRectangle(cornerRadius: 16), style: FillStyle())
                    
                    Spacer()
                    
                    VStack(alignment: .center) {
                        Spacer()
                        
                        VStack {
                            Text(networkClient.timeZone != nil ? "Использовано памяти" : "")
                                .font(.system(size: 16))
                                .padding(.bottom, 2)
                            Text(networkClient.timeZone ?? "")
                                .font(.system(size: 40, weight: .bold))
                                .italic()
                                .foregroundStyle(.cyan)
                        }
                        
                        Spacer()
                        
                        VStack {
                            Text(networkClient.durationOfWork != nil ? "Пользовательское время CPU:" : "")
                                .font(.system(size: 16))
                                .padding(.bottom, 2)
                            Text(networkClient.durationOfWork ?? "")
                                .font(.system(size: 40))
                                .italic()
                                .foregroundStyle(.cyan)
                        }
                        
                        Spacer()
                        
                    }
                    
                    Spacer()
                    
                    
                    HStack() {
                        Button(systemServerConnected ? "Отключиться" : "Подключиться") {
                            if systemServerConnected {
                                networkClient.disconnectTimeServer()
                            } else {
                                networkClient.connectTimeServer()
                            }
                            withAnimation {
                                systemServerConnected.toggle()
                            }
                        }
                        .buttonStyle(
                            GrowingButton(
                                color: systemServerConnected ? .red : .green.opacity(0.7)
                            )
                        )
                    }
                    .padding(.horizontal, 16)
                }
                .padding()
            }
            .padding(30)
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
        .frame(width: 800, height: 600)
        .frame(maxWidth: 800, maxHeight: 600)
    }
}

struct GrowingButton: ButtonStyle {
    let color: Color
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .padding()
            .font(.system(size: 16, weight: .medium))
            .frame(maxWidth: .infinity)
            .background(color)
            .foregroundStyle(.white)
            .clipShape(RoundedRectangle(cornerRadius: 16))
            .scaleEffect(configuration.isPressed ? 0.95 : 1)
            .opacity(configuration.isPressed ? 0.7 : 1)
            .animation(.easeOut(duration: 0.2), value: configuration.isPressed)
    }
}

#Preview {
    ContentView()
}
