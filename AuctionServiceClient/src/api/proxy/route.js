export const dynamic = 'force-dynamic';

export async function POST(request) {
  const corsHeaders = {
    'Access-Control-Allow-Origin': '*',
    'Access-Control-Allow-Methods': 'GET, POST, PUT, DELETE, OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type, Authorization, X-Requested-With',
    'Access-Control-Max-Age': '86400',
  };

  try {
    const { url, options = {} } = await request.json();

    if (!url) {
      return new Response(
        JSON.stringify({ 
          error: 'URL is required',
          message: 'Please provide a URL in the request body' 
        }),
        { 
          status: 400, 
          headers: { 
            'Content-Type': 'application/json',
            ...corsHeaders
          } 
        }
      );
    }

    // Валидация URL
    let targetUrl;
    try {
      targetUrl = new URL(url);
    } catch (e) {
      return new Response(
        JSON.stringify({ 
          error: 'Invalid URL',
          message: 'The provided URL is not valid' 
        }),
        { 
          status: 400, 
          headers: { 
            'Content-Type': 'application/json',
            ...corsHeaders
          } 
        }
      );
    }

    // Запрещаем запросы к локальным адресам в продакшене
    const isLocal = targetUrl.hostname === 'localhost' || 
                    targetUrl.hostname === '127.0.0.1' ||
                    targetUrl.hostname.endsWith('.local');
    
    if (process.env.NODE_ENV === 'production' && isLocal) {
      return new Response(
        JSON.stringify({ 
          error: 'Local URLs not allowed',
          message: 'Cannot proxy to local addresses in production' 
        }),
        { 
          status: 403, 
          headers: { 
            'Content-Type': 'application/json',
            ...corsHeaders
          } 
        }
      );
    }

    // Извлекаем опции запроса
    const { 
      method = 'GET', 
      headers = {}, 
      body,
      ...restOptions 
    } = options;

    // Подготавливаем headers для целевого запроса
    const targetHeaders = {
      'Accept': 'application/json',
      'User-Agent': 'Vercel-CORS-Proxy/1.0',
      ...headers
    };

    // Удаляем потенциально проблемные headers
    delete targetHeaders['host'];
    delete targetHeaders['origin'];
    delete targetHeaders['referer'];

    // Подготавливаем опции для fetch
    const fetchOptions = {
      method,
      headers: targetHeaders,
      ...restOptions,
      // Добавляем timeout
      signal: AbortSignal.timeout(30000) // 30 секунд timeout
    };

    // Добавляем body если есть
    if (body && method !== 'GET' && method !== 'HEAD') {
      if (typeof body === 'string') {
        fetchOptions.body = body;
      } else {
        fetchOptions.body = JSON.stringify(body);
        targetHeaders['Content-Type'] = 'application/json';
      }
    }

    // Выполняем запрос
    const response = await fetch(targetUrl.toString(), fetchOptions);
    
    // Получаем response text
    const responseText = await response.text();
    
    // Парсим JSON если возможно
    let responseData;
    try {
      responseData = responseText ? JSON.parse(responseText) : null;
    } catch (e) {
      responseData = { 
        _proxy_note: 'Response is not valid JSON, returning as text',
        text: responseText 
      };
    }

    // Формируем ответ
    return new Response(JSON.stringify(responseData), {
      status: response.status,
      statusText: response.statusText,
      headers: {
        'Content-Type': 'application/json',
        ...corsHeaders,
        'X-Proxy-By': 'Vercel-CORS-Proxy',
        'X-Target-URL': targetUrl.toString(),
        'X-Target-Status': response.status.toString()
      }
    });

  } catch (error) {
    console.error('Proxy error details:', {
      name: error.name,
      message: error.message,
      stack: error.stack,
      cause: error.cause
    });

    // Обработка различных типов ошибок
    let status = 500;
    let errorMessage = 'Internal proxy error';
    
    if (error.name === 'AbortError') {
      status = 504;
      errorMessage = 'Request timeout';
    } else if (error.name === 'TypeError' && error.message.includes('fetch')) {
      status = 502;
      errorMessage = 'Network error or invalid URL';
    }

    return new Response(
      JSON.stringify({ 
        error: errorMessage,
        message: error.message,
        type: error.name
      }),
      { 
        status,
        headers: { 
          'Content-Type': 'application/json',
          ...corsHeaders
        }
      }
    );
  }
}

// Обработка OPTIONS запроса для CORS preflight
export async function OPTIONS(request) {
  return new Response(null, {
    status: 200,
    headers: {
      'Access-Control-Allow-Origin': '*',
      'Access-Control-Allow-Methods': 'GET, POST, PUT, DELETE, OPTIONS',
      'Access-Control-Allow-Headers': 'Content-Type, Authorization, X-Requested-With',
      'Access-Control-Max-Age': '86400',
      'Access-Control-Allow-Credentials': 'true'
    }
  });
}

// Также можно добавить GET метод для тестирования
export async function GET(request) {
  return new Response(
    JSON.stringify({
      service: 'Vercel CORS Proxy',
      version: '1.0',
      endpoints: {
        POST: '/api/proxy - Proxy any HTTP request',
        GET: '/api/proxy - This info page'
      },
      usage: {
        example: `await fetch('/api/proxy', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    url: 'https://api.example.com/data',
    options: { method: 'GET' }
  })
})`
      }
    }),
    {
      status: 200,
      headers: {
        'Content-Type': 'application/json',
        'Access-Control-Allow-Origin': '*'
      }
    }
  );
}