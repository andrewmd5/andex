@ctype mat4 HMM_Mat4
@ctype vec4 HMM_Vec4
@ctype vec3 HMM_Vec3


@vs cursor_vs
in vec2 position;
out vec2 uv;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    uv = position;
}
@end

@fs cursor_fs
layout(binding=0) uniform globals {
    vec3  iResolution;
    float iTime;
    float iTimeDelta;
    float iFrameRate;
    int   iFrame;
    vec4  iChannelTime;
    vec4  iChannelResolution[4];
    vec4  iMouse;
    vec4  iDate;
    float iSampleRate;
    vec4  iCurrentCursor;
    vec4  iPreviousCursor;
    vec4  iCurrentCursorColor;
    vec4  iPreviousCursorColor;
    float iTimeCursorChange;
};

in vec2 uv;
out vec4 frag_color;

float getSdfRectangle(in vec2 p, in vec2 xy, in vec2 b) {
    vec2 d = abs(p - xy) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float seg(in vec2 p, in vec2 a, in vec2 b, inout float s, float d) {
    vec2 e = b - a;
    vec2 w = p - a;
    vec2 proj = a + e * clamp(dot(w, e) / dot(e, e), 0.0, 1.0);
    float segd = dot(p - proj, p - proj);
    d = min(d, segd);
    float c0 = step(0.0, p.y - a.y);
    float c1 = 1.0 - step(0.0, p.y - b.y);
    float c2 = 1.0 - step(0.0, e.x * w.y - e.y * w.x);
    float allCond = c0 * c1 * c2;
    float noneCond = (1.0 - c0) * (1.0 - c1) * (1.0 - c2);
    float flip = mix(1.0, -1.0, step(0.5, allCond + noneCond));
    s *= flip;
    return d;
}

float getSdfParallelogram(in vec2 p, in vec2 v0, in vec2 v1, in vec2 v2, in vec2 v3) {
    float s = 1.0;
    float d = dot(p - v0, p - v0);
    d = seg(p, v0, v3, s, d);
    d = seg(p, v1, v0, s, d);
    d = seg(p, v2, v1, s, d);
    d = seg(p, v3, v2, s, d);
    return s * sqrt(d);
}

vec2 normalize2(vec2 value, float isPosition) {
    return (value * 2.0 - (iResolution.xy * isPosition)) / iResolution.y;
}

float antialising(float distance) {
    return 1.0 - smoothstep(0.0, normalize2(vec2(2.0, 2.0), 0.0).x, distance);
}

float determineStartVertexFactor(vec2 a, vec2 b) {
    float condition1 = step(b.x, a.x) * step(a.y, b.y);
    float condition2 = step(a.x, b.x) * step(b.y, a.y);
    return 1.0 - max(condition1, condition2);
}

vec2 getRectangleCenter(vec4 rectangle) {
    return vec2(rectangle.x + (rectangle.z / 2.0), rectangle.y - (rectangle.w / 2.0));
}

float ease(float x) {
    return pow(1.0 - x, 3.0);
}

vec4 saturate(vec4 color, float factor) {
    float gray = dot(color, vec4(0.299, 0.587, 0.114, 0.0));
    return mix(vec4(gray, gray, gray, color.a), color, factor);
}

void main() {
    
    vec2 fragCoord = (uv + 1.0) * 0.5 * iResolution.xy;
    
    vec4 TRAIL_COLOR = iCurrentCursorColor;
    float OPACITY = 0.6;
    float DURATION = 0.3;
    float BLINK_SPEED = 1.0; 
    float STATIONARY_THRESHOLD = 0.5; 
    
    
    frag_color = vec4(0.0);
    
    
    vec2 vu = normalize2(fragCoord, 1.0);
    
    
    vec4 currentCursor = vec4(normalize2(iCurrentCursor.xy, 1.0), normalize2(iCurrentCursor.zw, 0.0));
    vec4 previousCursor = vec4(normalize2(iPreviousCursor.xy, 1.0), normalize2(iPreviousCursor.zw, 0.0));
    
    
    float timeSinceMove = iTime - iTimeCursorChange;
    float isStationary = step(STATIONARY_THRESHOLD, timeSinceMove);
    
    
    float blinkFactor = (sin(iTime * BLINK_SPEED * 6.28318) * 0.5 + 0.5);
    
    blinkFactor = smoothstep(0.3, 0.7, blinkFactor);
    
    
    float vertexFactor = determineStartVertexFactor(currentCursor.xy, previousCursor.xy);
    float invertedVertexFactor = 1.0 - vertexFactor;
    
    
    vec2 v0 = vec2(currentCursor.x + currentCursor.z * vertexFactor, currentCursor.y + currentCursor.w);
    vec2 v1 = vec2(currentCursor.x + currentCursor.z * invertedVertexFactor, currentCursor.y);
    vec2 v2 = vec2(previousCursor.x + currentCursor.z * invertedVertexFactor, previousCursor.y);
    vec2 v3 = vec2(previousCursor.x + currentCursor.z * vertexFactor, previousCursor.y + previousCursor.w);
    
    
    vec2 cursorCenter = vec2(currentCursor.x + currentCursor.z * 0.5, currentCursor.y + currentCursor.w * 0.5);
    float sdfCurrentCursor = getSdfRectangle(vu, cursorCenter, currentCursor.zw * 0.5);
    float sdfTrail = getSdfParallelogram(vu, v0, v1, v2, v3);
    
    float progress = clamp((iTime - iTimeCursorChange) / DURATION, 0.0, 1.0);
    float easedProgress = ease(progress);
    
    
    vec2 centerCC = getRectangleCenter(currentCursor);
    vec2 centerCP = getRectangleCenter(previousCursor);
    float lineLength = distance(centerCC, centerCP);
    
    vec4 newColor = vec4(0.0);
    vec4 trail = saturate(TRAIL_COLOR, 2.5);
    
    
    float trailVisibility = 1.0 - isStationary;
    newColor = mix(newColor, trail, antialising(sdfTrail) * trailVisibility);
    
    
    vec4 cursorColor = trail;
    float cursorAlpha = mix(1.0, blinkFactor, isStationary);
    cursorColor.a *= cursorAlpha;
    
    newColor = mix(newColor, cursorColor, antialising(sdfCurrentCursor));
    
    
    frag_color = mix(vec4(0.0), newColor, step(sdfCurrentCursor, easedProgress * lineLength + (1.0 - trailVisibility)));
    
    
    if (sdfCurrentCursor <= 0.0) {
        frag_color = cursorColor;
    }
}
@end

@program cursor cursor_vs cursor_fs


@vs selection_vs
in vec2 position;
in vec4 color0;
out vec4 color;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    color = color0;
}
@end

@fs selection_fs
in vec4 color;
out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

@program selection selection_vs selection_fs