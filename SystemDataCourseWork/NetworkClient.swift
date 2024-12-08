//
//  NetworkClient.swift
//  SystemDataCourseWork
//
//  Created by Александр Плешаков on 08.12.2024.
//

import Foundation
import Network

enum ReceivingType {
    case time, systemData
}

final class NetworkClient: ObservableObject {
    private let host = "127.0.0.1"
    private let timePort: UInt32 = 8080
    private let systemDataPort: UInt32 = 8081
    
    private var timeServerConnection: NWConnection?
    private var systemDataServerConnection: NWConnection?
    
    @Published var timeZone: String?
    @Published var durationOfWork: String?
    @Published var lastReceiveFromTimeServer: String?
    
    @Published var freeMemory: String?
    @Published var userModeWorkTime: String?
    @Published var lastReceiveFromSystemDataServer: String?
    
    func connectTimeServer() {
        connectToServer(host: host, port: timePort)
    }
    
    func disconnectTimeServer() {
        timeServerConnection?.cancel()
        timeServerConnection = nil
        print("Disconnected from server")
    }
    
    func connectSystemDataServer() {
        connectToServer(host: host, port: systemDataPort)
    }
    
    func disconnectSystemDataServer() {
        systemDataServerConnection?.cancel()
        systemDataServerConnection = nil
        print("Disconnected from server")
    }

    // Подключение к серверу
    private func connectToServer(host: String, port: UInt32) {
        if port == 8080 {
            timeServerConnection = NWConnection(
                host: NWEndpoint.Host(host),
                port: NWEndpoint.Port(rawValue: UInt16(port))!,
                using: .tcp
            )
            
            timeServerConnection?.stateUpdateHandler = { state in
                switch state {
                case .ready:
                    print("Connected to \(host):\(port)")
                    self.startReceiving(type: .time)
                case .failed(let error):
                    print("Connection failed: \(error)")
                default:
                    break
                }
            }
            
            timeServerConnection?.start(queue: .global())
        } else if port == 8081 {
            systemDataServerConnection = NWConnection(
                host: NWEndpoint.Host(host),
                port: NWEndpoint.Port(rawValue: UInt16(port))!,
                using: .tcp
            )
            
            systemDataServerConnection?.stateUpdateHandler = { state in
                switch state {
                case .ready:
                    print("Connected to \(host):\(port)")
                    self.startReceiving(type: .systemData)
                case .failed(let error):
                    print("Connection failed: \(error)")
                default:
                    break
                }
            }
            
            systemDataServerConnection?.start(queue: .global())
        }
    }
    
    // Непрерывное прослушивание данных
    private func startReceiving(type: ReceivingType) {
        let connection: NWConnection?
        switch type {
        case .time:
            connection = timeServerConnection
        case .systemData:
            connection = systemDataServerConnection
        }
        
        connection?.receive(minimumIncompleteLength: 1, maximumLength: 1024) { [weak self] data, _, isComplete, error in
            guard let self = self else { return }
            
            if let data = data, !data.isEmpty {
                let response = String(decoding: data, as: UTF8.self)
                print("Received: \(response)")
                
                switch type {
                case .time:
                    DispatchQueue.main.async { [weak self] in
                        guard let self else { return }
                        (timeZone, durationOfWork, lastReceiveFromTimeServer) = parseTimeServerResponse(response) ?? (nil, nil, nil)
                    }
                case .systemData:
                    DispatchQueue.main.async { [weak self] in
                        guard let self else { return }
                        (freeMemory, userModeWorkTime, lastReceiveFromSystemDataServer) = parseSystemDataServerResponse(response) ?? (nil, nil, nil)
                    }
                }
                
            } else if let error = error {
                print("Error receiving data: \(error)")
            }
            
            if isComplete {
                print("Connection closed by server")
                connection?.cancel()
            } else {
                // Слушаем снова
                self.startReceiving(type: type)
            }
        }
    }
    
    private func parseTimeServerResponse(
        _ response: String?
    ) -> (currentTimezone: String, sessionDuration: String, currentTime: String)? {
        guard let response else { return nil }
        let lines = response.components(separatedBy: "\n")
        
        guard lines.count >= 3 else {
            print("Invalid response format")
            return nil
        }
        
        let timezoneLine = lines[0]
        let sessionLine = lines[1]
        let timeLine = lines[2]
        
        let currentTimezone = timezoneLine.components(separatedBy: ": ").last ?? "N/A"
        let sessionDuration = sessionLine.components(separatedBy: ": ").last ?? "N/A"
        let currentTime = timeLine.components(separatedBy: ": ").last ?? "N/A"
        
        return (currentTimezone: currentTimezone, sessionDuration: sessionDuration, currentTime: currentTime)
    }
    
    private func parseSystemDataServerResponse(
        _ response: String?
    ) -> (memory: String, userModeTime: String, currentTime: String)? {
        guard let response else { return nil }
        let lines = response.components(separatedBy: "\n")
        
        guard lines.count >= 3 else {
            print("Invalid response format")
            return nil
        }
        
        let memoryLine = lines[0]
        let userModeTimeLine = lines[1]
        let timeLine = lines[2]
        
        let memory = memoryLine.components(separatedBy: ": ").last ?? "N/A"
        let userModeTime = userModeTimeLine.components(separatedBy: ": ").last ?? "N/A"
        let currentTime = timeLine.components(separatedBy: ": ").last ?? "N/A"
        
        return (memory: memory, userModeTime: userModeTime, currentTime: currentTime)
    }
}

