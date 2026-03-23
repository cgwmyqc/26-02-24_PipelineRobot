import axios from 'axios'
import { appConfig } from '../config/app'

const request = axios.create({
  baseURL: appConfig.backendBaseURL,
  timeout: 10000
})

export default request
